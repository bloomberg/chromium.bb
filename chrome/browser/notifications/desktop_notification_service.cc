// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_service.h"

#include "base/metrics/histogram.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_child_process_host.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using WebKit::WebNotificationPresenter;
using WebKit::WebTextDirection;

const ContentSetting kDefaultSetting = CONTENT_SETTING_ASK;

// NotificationPermissionCallbackTask -----------------------------------------

// A task object which calls the renderer to inform the web page that the
// permission request has completed.
class NotificationPermissionCallbackTask : public Task {
 public:
  NotificationPermissionCallbackTask(int process_id,
                                     int route_id,
                                     int request_id);
  virtual ~NotificationPermissionCallbackTask();

 private:
  virtual void Run();

  int process_id_;
  int route_id_;
  int request_id_;
};

NotificationPermissionCallbackTask::NotificationPermissionCallbackTask(
    int process_id,
    int route_id,
    int request_id)
    : process_id_(process_id),
      route_id_(route_id),
      request_id_(request_id) {
}

NotificationPermissionCallbackTask::~NotificationPermissionCallbackTask() {
}

void NotificationPermissionCallbackTask::Run() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RenderViewHost* host = RenderViewHost::FromID(process_id_, route_id_);
  if (host)
    host->Send(new ViewMsg_PermissionRequestDone(route_id_, request_id_));
}


// NotificationPermissionInfoBarDelegate --------------------------------------

// The delegate for the infobar shown when an origin requests notification
// permissions.
class NotificationPermissionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  NotificationPermissionInfoBarDelegate(TabContents* contents,
                                        const GURL& origin,
                                        const string16& display_name,
                                        int process_id,
                                        int route_id,
                                        int callback_context);

 private:
  virtual ~NotificationPermissionInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual void InfoBarClosed();
  virtual SkBitmap* GetIcon() const;
  virtual Type GetInfoBarType() const;
  virtual string16 GetMessageText() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();
  virtual bool Cancel();

  // The origin we are asking for permissions on.
  GURL origin_;

  // The display name for the origin to be displayed.  Will be different from
  // origin_ for extensions.
  string16 display_name_;

  // The Profile that we restore sessions from.
  Profile* profile_;

  // The callback information that tells us how to respond to javascript via
  // the correct RenderView.
  int process_id_;
  int route_id_;
  int callback_context_;

  // Whether the user clicked one of the buttons.
  bool action_taken_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPermissionInfoBarDelegate);
};

NotificationPermissionInfoBarDelegate::NotificationPermissionInfoBarDelegate(
    TabContents* contents,
    const GURL& origin,
    const string16& display_name,
    int process_id,
    int route_id,
    int callback_context)
    : ConfirmInfoBarDelegate(contents),
      origin_(origin),
      display_name_(display_name),
      profile_(contents->profile()),
      process_id_(process_id),
      route_id_(route_id),
      callback_context_(callback_context),
      action_taken_(false) {
}

NotificationPermissionInfoBarDelegate::
    ~NotificationPermissionInfoBarDelegate() {
}

void NotificationPermissionInfoBarDelegate::InfoBarClosed() {
  if (!action_taken_)
    UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Ignored", 1);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      new NotificationPermissionCallbackTask(process_id_, route_id_,
                                             callback_context_));

  delete this;
}

SkBitmap* NotificationPermissionInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
     IDR_PRODUCT_ICON_32);
}

InfoBarDelegate::Type
    NotificationPermissionInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 NotificationPermissionInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_NOTIFICATION_PERMISSIONS,
                                    display_name_);
}

string16 NotificationPermissionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_NOTIFICATION_PERMISSION_YES : IDS_NOTIFICATION_PERMISSION_NO);
}

bool NotificationPermissionInfoBarDelegate::Accept() {
  UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Allowed", 1);
  profile_->GetDesktopNotificationService()->GrantPermission(origin_);
  action_taken_ = true;
  return true;
}

bool NotificationPermissionInfoBarDelegate::Cancel() {
  UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Denied", 1);
  profile_->GetDesktopNotificationService()->DenyPermission(origin_);
  action_taken_ = true;
  return true;
}


// DesktopNotificationService -------------------------------------------------

// static
string16 DesktopNotificationService::CreateDataUrl(
    const GURL& icon_url, const string16& title, const string16& body,
    WebTextDirection dir) {
  int resource;
  std::vector<std::string> subst;
  if (icon_url.is_valid()) {
    resource = IDR_NOTIFICATION_ICON_HTML;
    subst.push_back(icon_url.spec());
    subst.push_back(EscapeForHTML(UTF16ToUTF8(title)));
    subst.push_back(EscapeForHTML(UTF16ToUTF8(body)));
    // icon float position
    subst.push_back(dir == WebKit::WebTextDirectionRightToLeft ?
                    "right" : "left");
  } else if (title.empty() || body.empty()) {
    resource = IDR_NOTIFICATION_1LINE_HTML;
    string16 line = title.empty() ? body : title;
    // Strings are div names in the template file.
    string16 line_name = title.empty() ? ASCIIToUTF16("description")
                                       : ASCIIToUTF16("title");
    subst.push_back(EscapeForHTML(UTF16ToUTF8(line_name)));
    subst.push_back(EscapeForHTML(UTF16ToUTF8(line)));
  } else {
    resource = IDR_NOTIFICATION_2LINE_HTML;
    subst.push_back(EscapeForHTML(UTF16ToUTF8(title)));
    subst.push_back(EscapeForHTML(UTF16ToUTF8(body)));
  }
  // body text direction
  subst.push_back(dir == WebKit::WebTextDirectionRightToLeft ?
                  "rtl" : "ltr");

  return CreateDataUrl(resource, subst);
}

// static
string16 DesktopNotificationService::CreateDataUrl(
    int resource, const std::vector<std::string>& subst) {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          resource));

  if (template_html.empty()) {
    NOTREACHED() << "unable to load template. ID: " << resource;
    return string16();
  }

  std::string data = ReplaceStringPlaceholders(template_html, subst, NULL);
  return UTF8ToUTF16("data:text/html;charset=utf-8," +
                      EscapeQueryParamValue(data, false));
}

DesktopNotificationService::DesktopNotificationService(Profile* profile,
    NotificationUIManager* ui_manager)
    : profile_(profile),
      ui_manager_(ui_manager) {
  prefs_registrar_.Init(profile_->GetPrefs());
  InitPrefs();
  StartObserving();
}

DesktopNotificationService::~DesktopNotificationService() {
  StopObserving();
}

void DesktopNotificationService::RegisterUserPrefs(PrefService* user_prefs) {
  if (!user_prefs->FindPreference(
      prefs::kDesktopNotificationDefaultContentSetting)) {
    user_prefs->RegisterIntegerPref(
        prefs::kDesktopNotificationDefaultContentSetting, kDefaultSetting);
  }
  if (!user_prefs->FindPreference(prefs::kDesktopNotificationAllowedOrigins))
    user_prefs->RegisterListPref(prefs::kDesktopNotificationAllowedOrigins);
  if (!user_prefs->FindPreference(prefs::kDesktopNotificationDeniedOrigins))
    user_prefs->RegisterListPref(prefs::kDesktopNotificationDeniedOrigins);
}

// Initialize the cache with the allowed and denied origins, or
// create the preferences if they don't exist yet.
void DesktopNotificationService::InitPrefs() {
  PrefService* prefs = profile_->GetPrefs();
  std::vector<GURL> allowed_origins;
  std::vector<GURL> denied_origins;
  ContentSetting default_content_setting = CONTENT_SETTING_DEFAULT;

  if (!profile_->IsOffTheRecord()) {
    default_content_setting = IntToContentSetting(
        prefs->GetInteger(prefs::kDesktopNotificationDefaultContentSetting));
    allowed_origins = GetAllowedOrigins();
    denied_origins = GetBlockedOrigins();
  }

  prefs_cache_ = new NotificationsPrefsCache();
  prefs_cache_->SetCacheDefaultContentSetting(default_content_setting);
  prefs_cache_->SetCacheAllowedOrigins(allowed_origins);
  prefs_cache_->SetCacheDeniedOrigins(denied_origins);
  prefs_cache_->set_is_initialized(true);
}

void DesktopNotificationService::StartObserving() {
  if (!profile_->IsOffTheRecord()) {
    prefs_registrar_.Add(prefs::kDesktopNotificationDefaultContentSetting,
                         this);
    prefs_registrar_.Add(prefs::kDesktopNotificationAllowedOrigins, this);
    prefs_registrar_.Add(prefs::kDesktopNotificationDeniedOrigins, this);

    notification_registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                                NotificationService::AllSources());
  }

  notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));
}

void DesktopNotificationService::StopObserving() {
  if (!profile_->IsOffTheRecord()) {
    prefs_registrar_.RemoveAll();
  }
  notification_registrar_.RemoveAll();
}

void DesktopNotificationService::GrantPermission(const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PersistPermissionChange(origin, true);

  // Schedule a cache update on the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          prefs_cache_.get(), &NotificationsPrefsCache::CacheAllowedOrigin,
          origin));

  NotifySettingsChange();
}

void DesktopNotificationService::DenyPermission(const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PersistPermissionChange(origin, false);

  // Schedule a cache update on the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          prefs_cache_.get(), &NotificationsPrefsCache::CacheDeniedOrigin,
          origin));

  NotifySettingsChange();
}

void DesktopNotificationService::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  if (NotificationType::PREF_CHANGED == type) {
    const std::string& name = *Details<std::string>(details).ptr();
    OnPrefsChanged(name);
  } else if (NotificationType::EXTENSION_UNLOADED == type) {
    // Remove all notifications currently shown or queued by the extension
    // which was unloaded.
    const Extension* extension =
        Details<UnloadedExtensionInfo>(details)->extension;
    if (extension)
      ui_manager_->CancelAllBySourceOrigin(extension->url());
  } else if (NotificationType::PROFILE_DESTROYED == type) {
    StopObserving();
  }
}

void DesktopNotificationService::OnPrefsChanged(const std::string& pref_name) {
  PrefService* prefs = profile_->GetPrefs();

  if (pref_name == prefs::kDesktopNotificationAllowedOrigins) {
    NotifySettingsChange();

    std::vector<GURL> allowed_origins(GetAllowedOrigins());
    // Schedule a cache update on the IO thread.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(
            prefs_cache_.get(),
            &NotificationsPrefsCache::SetCacheAllowedOrigins,
            allowed_origins));
  } else if (pref_name == prefs::kDesktopNotificationDeniedOrigins) {
    NotifySettingsChange();

    std::vector<GURL> denied_origins(GetBlockedOrigins());
    // Schedule a cache update on the IO thread.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(
            prefs_cache_.get(),
            &NotificationsPrefsCache::SetCacheDeniedOrigins,
            denied_origins));
  } else if (pref_name == prefs::kDesktopNotificationDefaultContentSetting) {
    NotificationService::current()->Notify(
        NotificationType::DESKTOP_NOTIFICATION_DEFAULT_CHANGED,
        Source<DesktopNotificationService>(this),
        NotificationService::NoDetails());

    const ContentSetting default_content_setting = IntToContentSetting(
        prefs->GetInteger(prefs::kDesktopNotificationDefaultContentSetting));

    // Schedule a cache update on the IO thread.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(
            prefs_cache_.get(),
            &NotificationsPrefsCache::SetCacheDefaultContentSetting,
            default_content_setting));
  }
}

void DesktopNotificationService::PersistPermissionChange(
    const GURL& origin, bool is_allowed) {
  // Don't persist changes when incognito.
  if (profile_->IsOffTheRecord())
    return;

  PrefService* prefs = profile_->GetPrefs();

  // |Observe()| updates the whole permission set in the cache, but only a
  // single origin has changed. Hence, callers of this method manually
  // schedule a task to update the prefs cache, and the prefs observer is
  // disabled while the update runs.
  StopObserving();

  bool allowed_changed = false;
  bool denied_changed = false;

  ListValue* allowed_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationAllowedOrigins);
  ListValue* denied_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationDeniedOrigins);
  {
    // value is passed to the preferences list, or deleted.
    StringValue* value = new StringValue(origin.spec());

    // Remove from one list and add to the other.
    if (is_allowed) {
      // Remove from the denied list.
      if (denied_sites->Remove(*value) != -1)
        denied_changed = true;

      // Add to the allowed list.
      if (allowed_sites->AppendIfNotPresent(value))
        allowed_changed = true;
      else
        delete value;
    } else {
      // Remove from the allowed list.
      if (allowed_sites->Remove(*value) != -1)
        allowed_changed = true;

      // Add to the denied list.
      if (denied_sites->AppendIfNotPresent(value))
        denied_changed = true;
      else
        delete value;
    }
  }

  // Persist the pref if anthing changed, but only send updates for the
  // list that changed.
  if (allowed_changed || denied_changed) {
    if (allowed_changed) {
      ScopedUserPrefUpdate update_allowed(
          prefs, prefs::kDesktopNotificationAllowedOrigins);
    }
    if (denied_changed) {
      ScopedUserPrefUpdate updateDenied(
          prefs, prefs::kDesktopNotificationDeniedOrigins);
    }
    prefs->ScheduleSavePersistentPrefs();
  }
  StartObserving();
}

ContentSetting DesktopNotificationService::GetDefaultContentSetting() {
  PrefService* prefs = profile_->GetPrefs();
  ContentSetting setting = IntToContentSetting(
      prefs->GetInteger(prefs::kDesktopNotificationDefaultContentSetting));
  if (setting == CONTENT_SETTING_DEFAULT)
    setting = kDefaultSetting;
  return setting;
}

void DesktopNotificationService::SetDefaultContentSetting(
    ContentSetting setting) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_->GetPrefs()->SetInteger(
      prefs::kDesktopNotificationDefaultContentSetting,
      setting == CONTENT_SETTING_DEFAULT ?  kDefaultSetting : setting);
  // The cache is updated through the notification observer.
}

bool DesktopNotificationService::IsDefaultContentSettingManaged() const {
  return profile_->GetPrefs()->IsManagedPreference(
      prefs::kDesktopNotificationDefaultContentSetting);
}

void DesktopNotificationService::ResetToDefaultContentSetting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PrefService* prefs = profile_->GetPrefs();
  prefs->ClearPref(prefs::kDesktopNotificationDefaultContentSetting);
}

std::vector<GURL> DesktopNotificationService::GetAllowedOrigins() {
  std::vector<GURL> allowed_origins;
  PrefService* prefs = profile_->GetPrefs();
  const ListValue* allowed_sites =
      prefs->GetList(prefs::kDesktopNotificationAllowedOrigins);
  if (allowed_sites) {
    NotificationsPrefsCache::ListValueToGurlVector(*allowed_sites,
                                                   &allowed_origins);
  }
  return allowed_origins;
}

std::vector<GURL> DesktopNotificationService::GetBlockedOrigins() {
  std::vector<GURL> denied_origins;
  PrefService* prefs = profile_->GetPrefs();
  const ListValue* denied_sites =
      prefs->GetList(prefs::kDesktopNotificationDeniedOrigins);
  if (denied_sites) {
    NotificationsPrefsCache::ListValueToGurlVector(*denied_sites,
                                                   &denied_origins);
  }
  return denied_origins;
}

void DesktopNotificationService::ResetAllowedOrigin(const GURL& origin) {
  if (profile_->IsOffTheRecord())
    return;

  // Since this isn't called often, let the normal observer behavior update the
  // cache in this case.
  PrefService* prefs = profile_->GetPrefs();
  ListValue* allowed_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationAllowedOrigins);
  {
    StringValue value(origin.spec());
    int removed_index = allowed_sites->Remove(value);
    DCHECK_NE(-1, removed_index) << origin << " was not allowed";
    ScopedUserPrefUpdate update_allowed(
        prefs, prefs::kDesktopNotificationAllowedOrigins);
  }
  prefs->ScheduleSavePersistentPrefs();
}

void DesktopNotificationService::ResetBlockedOrigin(const GURL& origin) {
  if (profile_->IsOffTheRecord())
    return;

  // Since this isn't called often, let the normal observer behavior update the
  // cache in this case.
  PrefService* prefs = profile_->GetPrefs();
  ListValue* denied_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationDeniedOrigins);
  {
    StringValue value(origin.spec());
    int removed_index = denied_sites->Remove(value);
    DCHECK_NE(-1, removed_index) << origin << " was not blocked";
    ScopedUserPrefUpdate update_allowed(
        prefs, prefs::kDesktopNotificationDeniedOrigins);
  }
  prefs->ScheduleSavePersistentPrefs();
}

void DesktopNotificationService::ResetAllOrigins() {
  PrefService* prefs = profile_->GetPrefs();
  prefs->ClearPref(prefs::kDesktopNotificationAllowedOrigins);
  prefs->ClearPref(prefs::kDesktopNotificationDeniedOrigins);
}

ContentSetting DesktopNotificationService::GetContentSetting(
    const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (profile_->IsOffTheRecord())
    return kDefaultSetting;

  std::vector<GURL> allowed_origins(GetAllowedOrigins());
  if (std::find(allowed_origins.begin(), allowed_origins.end(), origin) !=
      allowed_origins.end())
    return CONTENT_SETTING_ALLOW;

  std::vector<GURL> denied_origins(GetBlockedOrigins());
  if (std::find(denied_origins.begin(), denied_origins.end(), origin) !=
      denied_origins.end())
    return CONTENT_SETTING_BLOCK;

  return GetDefaultContentSetting();
}

void DesktopNotificationService::RequestPermission(
    const GURL& origin, int process_id, int route_id, int callback_context,
    TabContents* tab) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!tab)
    return;

  // If |origin| hasn't been seen before and the default content setting for
  // notifications is "ask", show an infobar.
  // The cache can only answer queries on the IO thread once it's initialized,
  // so don't ask the cache.
  ContentSetting setting = GetContentSetting(origin);
  if (setting == CONTENT_SETTING_ASK) {
    // Show an info bar requesting permission.
    tab->AddInfoBar(new NotificationPermissionInfoBarDelegate(
                        tab, origin, DisplayNameForOrigin(origin), process_id,
                        route_id, callback_context));
  } else {
    // Notify renderer immediately.
    BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      new NotificationPermissionCallbackTask(
          process_id, route_id, callback_context));
  }
}

void DesktopNotificationService::ShowNotification(
    const Notification& notification) {
  ui_manager_->Add(notification, profile_);
}

bool DesktopNotificationService::CancelDesktopNotification(
    int process_id, int route_id, int notification_id) {
  scoped_refptr<NotificationObjectProxy> proxy(
      new NotificationObjectProxy(process_id, route_id, notification_id,
                                  false));
  return ui_manager_->CancelById(proxy->id());
}


bool DesktopNotificationService::ShowDesktopNotification(
    const ViewHostMsg_ShowNotification_Params& params,
    int process_id, int route_id, DesktopNotificationSource source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const GURL& origin = params.origin;
  NotificationObjectProxy* proxy =
      new NotificationObjectProxy(process_id, route_id,
                                  params.notification_id,
                                  source == WorkerNotification);
  GURL contents;
  if (params.is_html) {
    contents = params.contents_url;
  } else {
    // "upconvert" the string parameters to a data: URL.
    contents = GURL(
        CreateDataUrl(params.icon_url, params.title, params.body,
                      params.direction));
  }
  Notification notification(
      origin, contents, DisplayNameForOrigin(origin),
      params.replace_id, proxy);
  ShowNotification(notification);
  return true;
}

string16 DesktopNotificationService::DisplayNameForOrigin(
    const GURL& origin) {
  // If the source is an extension, lookup the display name.
  if (origin.SchemeIs(chrome::kExtensionScheme)) {
    ExtensionService* ext_service = profile_->GetExtensionService();
    if (ext_service) {
      const Extension* extension = ext_service->GetExtensionByURL(origin);
      if (extension)
        return UTF8ToUTF16(extension->name());
    }
  }
  return UTF8ToUTF16(origin.host());
}

void DesktopNotificationService::NotifySettingsChange() {
  NotificationService::current()->Notify(
      NotificationType::DESKTOP_NOTIFICATION_SETTINGS_CHANGED,
      Source<DesktopNotificationService>(this),
      NotificationService::NoDetails());
}
