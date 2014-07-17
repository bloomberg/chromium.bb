// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_service.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/show_desktop_notification_params.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/message_center/notifier_settings.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/notifications/notifications_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#endif

using blink::WebTextDirection;
using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;
using message_center::NotifierId;

namespace {

const char kChromeNowExtensionID[] = "pafkbggdmjlpgkdkcbjmhmfcdpncadgh";

// NotificationPermissionRequest ---------------------------------------

class NotificationPermissionRequest : public PermissionBubbleRequest {
 public:
  NotificationPermissionRequest(
      DesktopNotificationService* notification_service,
      const GURL& origin,
      base::string16 display_name,
      const base::Closure& callback);
  virtual ~NotificationPermissionRequest();

  // PermissionBubbleDelegate:
  virtual int GetIconID() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetMessageTextFragment() const OVERRIDE;
  virtual bool HasUserGesture() const OVERRIDE;
  virtual GURL GetRequestingHostname() const OVERRIDE;
  virtual void PermissionGranted() OVERRIDE;
  virtual void PermissionDenied() OVERRIDE;
  virtual void Cancelled() OVERRIDE;
  virtual void RequestFinished() OVERRIDE;

 private:
  // The notification service to be used.
  DesktopNotificationService* notification_service_;

  // The origin we are asking for permissions on.
  GURL origin_;

  // The display name for the origin to be displayed.  Will be different from
  // origin_ for extensions.
  base::string16 display_name_;

  // The callback information that tells us how to respond to javascript.
  base::Closure callback_;

  // Whether the user clicked one of the buttons.
  bool action_taken_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPermissionRequest);
};

NotificationPermissionRequest::NotificationPermissionRequest(
    DesktopNotificationService* notification_service,
    const GURL& origin,
    base::string16 display_name,
    const base::Closure& callback)
    : notification_service_(notification_service),
      origin_(origin),
      display_name_(display_name),
      callback_(callback),
      action_taken_(false) {}

NotificationPermissionRequest::~NotificationPermissionRequest() {}

int NotificationPermissionRequest::GetIconID() const {
  return IDR_INFOBAR_DESKTOP_NOTIFICATIONS;
}

base::string16 NotificationPermissionRequest::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_NOTIFICATION_PERMISSIONS,
                                    display_name_);
}

base::string16
NotificationPermissionRequest::GetMessageTextFragment() const {
  return l10n_util::GetStringUTF16(IDS_NOTIFICATION_PERMISSIONS_FRAGMENT);
}

bool NotificationPermissionRequest::HasUserGesture() const {
  // Currently notification permission requests are only issued on
  // user gesture.
  return true;
}

GURL NotificationPermissionRequest::GetRequestingHostname() const {
  return origin_;
}

void NotificationPermissionRequest::PermissionGranted() {
  action_taken_ = true;
  UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Allowed", 1);
  notification_service_->GrantPermission(origin_);
}

void NotificationPermissionRequest::PermissionDenied() {
  action_taken_ = true;
  UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Denied", 1);
  notification_service_->DenyPermission(origin_);
}

void NotificationPermissionRequest::Cancelled() {
}

void NotificationPermissionRequest::RequestFinished() {
  if (!action_taken_)
    UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Ignored", 1);

  callback_.Run();

  delete this;
}


// NotificationPermissionInfoBarDelegate --------------------------------------

// The delegate for the infobar shown when an origin requests notification
// permissions.
class NotificationPermissionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a notification permission infobar and delegate and adds the infobar
  // to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     DesktopNotificationService* notification_service,
                     const GURL& origin,
                     const base::string16& display_name,
                     const base::Closure& callback);

 private:
  NotificationPermissionInfoBarDelegate(
      DesktopNotificationService* notification_service,
      const GURL& origin,
      const base::string16& display_name,
      const base::Closure& callback);
  virtual ~NotificationPermissionInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual int GetIconID() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // The origin we are asking for permissions on.
  GURL origin_;

  // The display name for the origin to be displayed.  Will be different from
  // origin_ for extensions.
  base::string16 display_name_;

  // The notification service to be used.
  DesktopNotificationService* notification_service_;

  // The callback information that tells us how to respond to javascript.
  base::Closure callback_;

  // Whether the user clicked one of the buttons.
  bool action_taken_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPermissionInfoBarDelegate);
};

// static
void NotificationPermissionInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    DesktopNotificationService* notification_service,
    const GURL& origin,
    const base::string16& display_name,
    const base::Closure& callback) {
  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(
          new NotificationPermissionInfoBarDelegate(
              notification_service, origin, display_name, callback))));
}

NotificationPermissionInfoBarDelegate::NotificationPermissionInfoBarDelegate(
    DesktopNotificationService* notification_service,
    const GURL& origin,
    const base::string16& display_name,
    const base::Closure& callback)
    : ConfirmInfoBarDelegate(),
      origin_(origin),
      display_name_(display_name),
      notification_service_(notification_service),
      callback_(callback),
      action_taken_(false) {
}

NotificationPermissionInfoBarDelegate::
    ~NotificationPermissionInfoBarDelegate() {
  if (!action_taken_)
    UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Ignored", 1);

  callback_.Run();
}

int NotificationPermissionInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_DESKTOP_NOTIFICATIONS;
}

infobars::InfoBarDelegate::Type
NotificationPermissionInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

base::string16 NotificationPermissionInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_NOTIFICATION_PERMISSIONS,
                                    display_name_);
}

base::string16 NotificationPermissionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_NOTIFICATION_PERMISSION_YES : IDS_NOTIFICATION_PERMISSION_NO);
}

bool NotificationPermissionInfoBarDelegate::Accept() {
  UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Allowed", 1);
  notification_service_->GrantPermission(origin_);
  action_taken_ = true;
  return true;
}

bool NotificationPermissionInfoBarDelegate::Cancel() {
  UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Denied", 1);
  notification_service_->DenyPermission(origin_);
  action_taken_ = true;
  return true;
}

void CancelNotification(const std::string& id) {
  g_browser_process->notification_ui_manager()->CancelById(id);
}

}  // namespace


// DesktopNotificationService -------------------------------------------------

// static
void DesktopNotificationService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(
      prefs::kMessageCenterDisabledExtensionIds,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      prefs::kMessageCenterDisabledSystemComponentIds,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  ExtensionWelcomeNotification::RegisterProfilePrefs(registry);
}

// static
base::string16 DesktopNotificationService::CreateDataUrl(
    const GURL& icon_url,
    const base::string16& title,
    const base::string16& body,
    WebTextDirection dir) {
  int resource;
  std::vector<std::string> subst;
  if (icon_url.is_valid()) {
    resource = IDR_NOTIFICATION_ICON_HTML;
    subst.push_back(icon_url.spec());
    subst.push_back(net::EscapeForHTML(base::UTF16ToUTF8(title)));
    subst.push_back(net::EscapeForHTML(base::UTF16ToUTF8(body)));
    // icon float position
    subst.push_back(dir == blink::WebTextDirectionRightToLeft ?
                    "right" : "left");
  } else if (title.empty() || body.empty()) {
    resource = IDR_NOTIFICATION_1LINE_HTML;
    base::string16 line = title.empty() ? body : title;
    // Strings are div names in the template file.
    base::string16 line_name =
        title.empty() ? base::ASCIIToUTF16("description")
                      : base::ASCIIToUTF16("title");
    subst.push_back(net::EscapeForHTML(base::UTF16ToUTF8(line_name)));
    subst.push_back(net::EscapeForHTML(base::UTF16ToUTF8(line)));
  } else {
    resource = IDR_NOTIFICATION_2LINE_HTML;
    subst.push_back(net::EscapeForHTML(base::UTF16ToUTF8(title)));
    subst.push_back(net::EscapeForHTML(base::UTF16ToUTF8(body)));
  }
  // body text direction
  subst.push_back(dir == blink::WebTextDirectionRightToLeft ?
                  "rtl" : "ltr");

  return CreateDataUrl(resource, subst);
}

// static
base::string16 DesktopNotificationService::CreateDataUrl(
    int resource, const std::vector<std::string>& subst) {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          resource));

  if (template_html.empty()) {
    NOTREACHED() << "unable to load template. ID: " << resource;
    return base::string16();
  }

  std::string data = ReplaceStringPlaceholders(template_html, subst, NULL);
  return base::UTF8ToUTF16("data:text/html;charset=utf-8," +
                               net::EscapeQueryParamValue(data, false));
}

// static
std::string DesktopNotificationService::AddIconNotification(
    const GURL& origin_url,
    const base::string16& title,
    const base::string16& message,
    const gfx::Image& icon,
    const base::string16& replace_id,
    NotificationDelegate* delegate,
    Profile* profile) {
  Notification notification(origin_url, icon, title, message,
                            blink::WebTextDirectionDefault,
                            base::string16(), replace_id, delegate);
  g_browser_process->notification_ui_manager()->Add(notification, profile);
  return notification.delegate_id();
}

DesktopNotificationService::DesktopNotificationService(
    Profile* profile,
    NotificationUIManager* ui_manager)
    : profile_(profile),
      ui_manager_(ui_manager) {
  OnStringListPrefChanged(
      prefs::kMessageCenterDisabledExtensionIds, &disabled_extension_ids_);
  OnStringListPrefChanged(
      prefs::kMessageCenterDisabledSystemComponentIds,
      &disabled_system_component_ids_);
  disabled_extension_id_pref_.Init(
      prefs::kMessageCenterDisabledExtensionIds,
      profile_->GetPrefs(),
      base::Bind(
          &DesktopNotificationService::OnStringListPrefChanged,
          base::Unretained(this),
          base::Unretained(prefs::kMessageCenterDisabledExtensionIds),
          base::Unretained(&disabled_extension_ids_)));
  disabled_system_component_id_pref_.Init(
      prefs::kMessageCenterDisabledSystemComponentIds,
      profile_->GetPrefs(),
      base::Bind(
          &DesktopNotificationService::OnStringListPrefChanged,
          base::Unretained(this),
          base::Unretained(prefs::kMessageCenterDisabledSystemComponentIds),
          base::Unretained(&disabled_system_component_ids_)));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED,
                 content::Source<Profile>(profile_));
}

DesktopNotificationService::~DesktopNotificationService() {
}

void DesktopNotificationService::GrantPermission(const GURL& origin) {
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURLNoWildcard(origin);
  profile_->GetHostContentSettingsMap()->SetContentSetting(
      primary_pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      NO_RESOURCE_IDENTIFIER,
      CONTENT_SETTING_ALLOW);
}

void DesktopNotificationService::DenyPermission(const GURL& origin) {
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURLNoWildcard(origin);
  profile_->GetHostContentSettingsMap()->SetContentSetting(
      primary_pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      NO_RESOURCE_IDENTIFIER,
      CONTENT_SETTING_BLOCK);
}

ContentSetting DesktopNotificationService::GetDefaultContentSetting(
    std::string* provider_id) {
  return profile_->GetHostContentSettingsMap()->GetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, provider_id);
}

void DesktopNotificationService::SetDefaultContentSetting(
    ContentSetting setting) {
  profile_->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, setting);
}

void DesktopNotificationService::ResetToDefaultContentSetting() {
  profile_->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_DEFAULT);
}

void DesktopNotificationService::GetNotificationsSettings(
    ContentSettingsForOneType* settings) {
  profile_->GetHostContentSettingsMap()->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      NO_RESOURCE_IDENTIFIER,
      settings);
}

void DesktopNotificationService::ClearSetting(
    const ContentSettingsPattern& pattern) {
  profile_->GetHostContentSettingsMap()->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      NO_RESOURCE_IDENTIFIER,
      CONTENT_SETTING_DEFAULT);
}

void DesktopNotificationService::ResetAllOrigins() {
  profile_->GetHostContentSettingsMap()->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

ContentSetting DesktopNotificationService::GetContentSetting(
    const GURL& origin) {
  return profile_->GetHostContentSettingsMap()
      ->GetContentSettingAndMaybeUpdateLastUsage(
          origin,
          origin,
          CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
          NO_RESOURCE_IDENTIFIER);
}

void DesktopNotificationService::RequestPermission(
    const GURL& origin,
    content::RenderFrameHost* render_frame_host,
    const base::Closure& callback) {
  // If |origin| hasn't been seen before and the default content setting for
  // notifications is "ask", show an infobar.
  // The cache can only answer queries on the IO thread once it's initialized,
  // so don't ask the cache.
  WebContents* web_contents = WebContents::FromRenderFrameHost(
      render_frame_host);
  ContentSetting setting = GetContentSetting(origin);
  if (setting == CONTENT_SETTING_ASK) {
    if (PermissionBubbleManager::Enabled()) {
      PermissionBubbleManager* bubble_manager =
          PermissionBubbleManager::FromWebContents(web_contents);
      if (bubble_manager) {
        bubble_manager->AddRequest(new NotificationPermissionRequest(
            this,
            origin,
            DisplayNameForOriginInProcessId(
                origin, render_frame_host->GetProcess()->GetID()),
            callback));
      }
      return;
    }

    // Show an info bar requesting permission.
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents);
    // |infobar_service| may be NULL, e.g., if this request originated in a
    // browser action popup, extension background page, or any HTML that runs
    // outside of a tab.
    if (infobar_service) {
      NotificationPermissionInfoBarDelegate::Create(
          infobar_service, this, origin,
          DisplayNameForOriginInProcessId(
              origin, render_frame_host->GetProcess()->GetID()),
          callback);
      return;
    }
  }

  // Notify renderer immediately.
  callback.Run();
}

void DesktopNotificationService::ShowDesktopNotification(
    const content::ShowDesktopNotificationHostMsgParams& params,
    content::RenderFrameHost* render_frame_host,
    content::DesktopNotificationDelegate* delegate,
    base::Closure* cancel_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const GURL& origin = params.origin;
  NotificationObjectProxy* proxy =
      new NotificationObjectProxy(render_frame_host, delegate);

  base::string16 display_source = DisplayNameForOriginInProcessId(
      origin, render_frame_host->GetProcess()->GetID());
  Notification notification(origin, params.icon_url, params.title,
      params.body, params.direction, display_source, params.replace_id,
      proxy);

  // The webkit notification doesn't timeout.
  notification.set_never_timeout(true);

  GetUIManager()->Add(notification, profile_);
  if (cancel_callback)
    *cancel_callback = base::Bind(&CancelNotification, proxy->id());
}

base::string16 DesktopNotificationService::DisplayNameForOriginInProcessId(
    const GURL& origin, int process_id) {
#if defined(ENABLE_EXTENSIONS)
  // If the source is an extension, lookup the display name.
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    extensions::InfoMap* extension_info_map =
        extensions::ExtensionSystem::Get(profile_)->info_map();
    if (extension_info_map) {
      extensions::ExtensionSet extensions;
      extension_info_map->GetExtensionsWithAPIPermissionForSecurityOrigin(
          origin, process_id, extensions::APIPermission::kNotification,
          &extensions);
      for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
           iter != extensions.end(); ++iter) {
        NotifierId notifier_id(NotifierId::APPLICATION, (*iter)->id());
        if (IsNotifierEnabled(notifier_id))
          return base::UTF8ToUTF16((*iter)->name());
      }
    }
  }
#endif

  return base::UTF8ToUTF16(origin.host());
}

void DesktopNotificationService::NotifySettingsChange() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_DESKTOP_NOTIFICATION_SETTINGS_CHANGED,
      content::Source<DesktopNotificationService>(this),
      content::NotificationService::NoDetails());
}

NotificationUIManager* DesktopNotificationService::GetUIManager() {
  // We defer setting ui_manager_ to the global singleton until we need it
  // in order to avoid UI dependent construction during startup.
  if (!ui_manager_)
    ui_manager_ = g_browser_process->notification_ui_manager();
  return ui_manager_;
}

bool DesktopNotificationService::IsNotifierEnabled(
    const NotifierId& notifier_id) {
  switch (notifier_id.type) {
    case NotifierId::APPLICATION:
      return disabled_extension_ids_.find(notifier_id.id) ==
          disabled_extension_ids_.end();
    case NotifierId::WEB_PAGE:
      return GetContentSetting(notifier_id.url) == CONTENT_SETTING_ALLOW;
    case NotifierId::SYSTEM_COMPONENT:
#if defined(OS_CHROMEOS)
      return disabled_system_component_ids_.find(notifier_id.id) ==
          disabled_system_component_ids_.end();
#else
      // We do not disable system component notifications.
      return true;
#endif
  }

  NOTREACHED();
  return false;
}

void DesktopNotificationService::SetNotifierEnabled(
    const NotifierId& notifier_id,
    bool enabled) {
  DCHECK_NE(NotifierId::WEB_PAGE, notifier_id.type);

  bool add_new_item = false;
  const char* pref_name = NULL;
  scoped_ptr<base::StringValue> id;
  switch (notifier_id.type) {
    case NotifierId::APPLICATION:
      pref_name = prefs::kMessageCenterDisabledExtensionIds;
      add_new_item = !enabled;
      id.reset(new base::StringValue(notifier_id.id));
      FirePermissionLevelChangedEvent(notifier_id, enabled);
      break;
    case NotifierId::SYSTEM_COMPONENT:
#if defined(OS_CHROMEOS)
      pref_name = prefs::kMessageCenterDisabledSystemComponentIds;
      add_new_item = !enabled;
      id.reset(new base::StringValue(notifier_id.id));
#else
      return;
#endif
      break;
    default:
      NOTREACHED();
  }
  DCHECK(pref_name != NULL);

  ListPrefUpdate update(profile_->GetPrefs(), pref_name);
  base::ListValue* const list = update.Get();
  if (add_new_item) {
    // AppendIfNotPresent will delete |adding_value| when the same value
    // already exists.
    list->AppendIfNotPresent(id.release());
  } else {
    list->Remove(*id, NULL);
  }
}

void DesktopNotificationService::ShowWelcomeNotificationIfNecessary(
    const Notification& notification) {
  if (!chrome_now_welcome_notification_) {
    chrome_now_welcome_notification_ =
        ExtensionWelcomeNotification::Create(kChromeNowExtensionID, profile_);
  }

  if (chrome_now_welcome_notification_) {
    chrome_now_welcome_notification_->ShowWelcomeNotificationIfNecessary(
        notification);
  }
}

void DesktopNotificationService::OnStringListPrefChanged(
    const char* pref_name, std::set<std::string>* ids_field) {
  ids_field->clear();
  // Separate GetPrefs()->GetList() to analyze the crash. See crbug.com/322320
  const PrefService* pref_service = profile_->GetPrefs();
  CHECK(pref_service);
  const base::ListValue* pref_list = pref_service->GetList(pref_name);
  for (size_t i = 0; i < pref_list->GetSize(); ++i) {
    std::string element;
    if (pref_list->GetString(i, &element) && !element.empty())
      ids_field->insert(element);
    else
      LOG(WARNING) << i << "-th element is not a string for " << pref_name;
  }
}

void DesktopNotificationService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(ENABLE_EXTENSIONS)
  DCHECK_EQ(chrome::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED, type);

  extensions::Extension* extension =
      content::Details<extensions::Extension>(details).ptr();
  NotifierId notifier_id(NotifierId::APPLICATION, extension->id());
  if (IsNotifierEnabled(notifier_id))
    return;

  // The settings for ephemeral apps will be persisted across cache evictions.
  if (extensions::util::IsEphemeralApp(extension->id(), profile_))
    return;

  SetNotifierEnabled(notifier_id, true);
#endif
}

void DesktopNotificationService::FirePermissionLevelChangedEvent(
    const NotifierId& notifier_id, bool enabled) {
#if defined(ENABLE_EXTENSIONS)
  DCHECK_EQ(NotifierId::APPLICATION, notifier_id.type);
  extensions::api::notifications::PermissionLevel permission =
      enabled ? extensions::api::notifications::PERMISSION_LEVEL_GRANTED
              : extensions::api::notifications::PERMISSION_LEVEL_DENIED;
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(new base::StringValue(
      extensions::api::notifications::ToString(permission)));
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::api::notifications::OnPermissionLevelChanged::kEventName,
      args.Pass()));
  extensions::EventRouter::Get(profile_)
      ->DispatchEventToExtension(notifier_id.id, event.Pass());

  // Tell the IO thread that this extension's permission for notifications
  // has changed.
  extensions::InfoMap* extension_info_map =
      extensions::ExtensionSystem::Get(profile_)->info_map();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&extensions::InfoMap::SetNotificationsDisabled,
                 extension_info_map, notifier_id.id, !enabled));
#endif
}
