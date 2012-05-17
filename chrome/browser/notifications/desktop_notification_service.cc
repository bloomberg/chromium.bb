// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_service.h"

#include "base/metrics/histogram.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/show_desktop_notification_params.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/base/escape.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;
using WebKit::WebNotificationPresenter;
using WebKit::WebTextDirection;
using WebKit::WebSecurityOrigin;

const ContentSetting kDefaultSetting = CONTENT_SETTING_ASK;

// NotificationPermissionInfoBarDelegate --------------------------------------

// The delegate for the infobar shown when an origin requests notification
// permissions.
class NotificationPermissionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  NotificationPermissionInfoBarDelegate(
      InfoBarTabHelper* infobar_helper,
      DesktopNotificationService* notification_service,
      const GURL& origin,
      const string16& display_name,
      int process_id,
      int route_id,
      int callback_context);

 private:
  virtual ~NotificationPermissionInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // The origin we are asking for permissions on.
  GURL origin_;

  // The display name for the origin to be displayed.  Will be different from
  // origin_ for extensions.
  string16 display_name_;

  // The notification service to be used.
  DesktopNotificationService* notification_service_;

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
    InfoBarTabHelper* infobar_helper,
    DesktopNotificationService* notification_service,
    const GURL& origin,
    const string16& display_name,
    int process_id,
    int route_id,
    int callback_context)
    : ConfirmInfoBarDelegate(infobar_helper),
      origin_(origin),
      display_name_(display_name),
      notification_service_(notification_service),
      process_id_(process_id),
      route_id_(route_id),
      callback_context_(callback_context),
      action_taken_(false) {
}

NotificationPermissionInfoBarDelegate::
    ~NotificationPermissionInfoBarDelegate() {
  if (!action_taken_)
    UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Ignored", 1);

  RenderViewHost* host = RenderViewHost::FromID(process_id_, route_id_);
  if (host)
    host->DesktopNotificationPermissionRequestDone(callback_context_);
}

gfx::Image* NotificationPermissionInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_DESKTOP_NOTIFICATIONS);
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
    subst.push_back(net::EscapeForHTML(UTF16ToUTF8(title)));
    subst.push_back(net::EscapeForHTML(UTF16ToUTF8(body)));
    // icon float position
    subst.push_back(dir == WebKit::WebTextDirectionRightToLeft ?
                    "right" : "left");
  } else if (title.empty() || body.empty()) {
    resource = IDR_NOTIFICATION_1LINE_HTML;
    string16 line = title.empty() ? body : title;
    // Strings are div names in the template file.
    string16 line_name = title.empty() ? ASCIIToUTF16("description")
                                       : ASCIIToUTF16("title");
    subst.push_back(net::EscapeForHTML(UTF16ToUTF8(line_name)));
    subst.push_back(net::EscapeForHTML(UTF16ToUTF8(line)));
  } else {
    resource = IDR_NOTIFICATION_2LINE_HTML;
    subst.push_back(net::EscapeForHTML(UTF16ToUTF8(title)));
    subst.push_back(net::EscapeForHTML(UTF16ToUTF8(body)));
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
          resource, ui::SCALE_FACTOR_NONE));

  if (template_html.empty()) {
    NOTREACHED() << "unable to load template. ID: " << resource;
    return string16();
  }

  std::string data = ReplaceStringPlaceholders(template_html, subst, NULL);
  return UTF8ToUTF16("data:text/html;charset=utf-8," +
                      net::EscapeQueryParamValue(data, false));
}

DesktopNotificationService::DesktopNotificationService(Profile* profile,
    NotificationUIManager* ui_manager)
    : profile_(profile),
      ui_manager_(ui_manager) {
  StartObserving();
}

DesktopNotificationService::~DesktopNotificationService() {
  StopObserving();
}

void DesktopNotificationService::StartObserving() {
  if (!profile_->IsOffTheRecord()) {
    notification_registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                                content::Source<Profile>(profile_));
  }
  notification_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::Source<Profile>(profile_));
}

void DesktopNotificationService::StopObserving() {
  notification_registrar_.RemoveAll();
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

void DesktopNotificationService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    // Remove all notifications currently shown or queued by the extension
    // which was unloaded.
    const Extension* extension =
        content::Details<UnloadedExtensionInfo>(details)->extension;
    if (extension)
      GetUIManager()->CancelAllBySourceOrigin(extension->url());
  } else if (type == chrome::NOTIFICATION_PROFILE_DESTROYED) {
    StopObserving();
  }
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
  return profile_->GetHostContentSettingsMap()->GetContentSetting(
      origin,
      origin,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      NO_RESOURCE_IDENTIFIER);
}

void DesktopNotificationService::RequestPermission(
    const GURL& origin, int process_id, int route_id, int callback_context,
    WebContents* tab) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!tab) {
    Browser* browser = browser::FindLastActiveWithProfile(profile_);
    if (browser)
      tab = browser->GetSelectedWebContents();
  }

  if (!tab)
    return;

  // If |origin| hasn't been seen before and the default content setting for
  // notifications is "ask", show an infobar.
  // The cache can only answer queries on the IO thread once it's initialized,
  // so don't ask the cache.
  ContentSetting setting = GetContentSetting(origin);
  if (setting == CONTENT_SETTING_ASK) {
    // Show an info bar requesting permission.
    TabContentsWrapper* wrapper =
        TabContentsWrapper::GetCurrentWrapperForContents(tab);
    InfoBarTabHelper* infobar_helper = wrapper->infobar_tab_helper();
    infobar_helper->AddInfoBar(new NotificationPermissionInfoBarDelegate(
        infobar_helper,
        DesktopNotificationServiceFactory::GetForProfile(wrapper->profile()),
        origin,
        DisplayNameForOrigin(origin),
        process_id,
        route_id,
        callback_context));
  } else {
    // Notify renderer immediately.
    RenderViewHost* host = RenderViewHost::FromID(process_id, route_id);
    if (host)
      host->DesktopNotificationPermissionRequestDone(callback_context);
  }
}

void DesktopNotificationService::ShowNotification(
    const Notification& notification) {
  GetUIManager()->Add(notification, profile_);
}

bool DesktopNotificationService::CancelDesktopNotification(
    int process_id, int route_id, int notification_id) {
  scoped_refptr<NotificationObjectProxy> proxy(
      new NotificationObjectProxy(process_id, route_id, notification_id,
                                  false));
  return GetUIManager()->CancelById(proxy->id());
}

bool DesktopNotificationService::ShowDesktopNotification(
    const content::ShowDesktopNotificationHostMsgParams& params,
    int process_id, int route_id, DesktopNotificationSource source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const GURL& origin = params.origin;
  NotificationObjectProxy* proxy =
      new NotificationObjectProxy(process_id, route_id,
                                  params.notification_id,
                                  source == WorkerNotification);

  string16 display_source = DisplayNameForOrigin(origin);
  if (params.is_html) {
    ShowNotification(Notification(origin, params.contents_url, display_source,
        params.replace_id, proxy));
  } else {
    ShowNotification(Notification(origin, params.icon_url, params.title,
        params.body, params.direction, display_source, params.replace_id,
        proxy));
  }
  return true;
}

string16 DesktopNotificationService::DisplayNameForOrigin(
    const GURL& origin) {
  // If the source is an extension, lookup the display name.
  if (origin.SchemeIs(chrome::kExtensionScheme)) {
    ExtensionService* extension_service = profile_->GetExtensionService();
    if (extension_service) {
      const Extension* extension =
          extension_service->extensions()->GetExtensionOrAppByURL(
              ExtensionURLInfo(
                  WebSecurityOrigin::createFromString(
                      UTF8ToUTF16(origin.spec())),
                  origin));
      if (extension)
        return UTF8ToUTF16(extension->name());
    }
  }
  return UTF8ToUTF16(origin.host());
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

WebKit::WebNotificationPresenter::Permission
    DesktopNotificationService::HasPermission(const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  HostContentSettingsMap* host_content_settings_map =
      profile_->GetHostContentSettingsMap();
  ContentSetting setting = host_content_settings_map->GetContentSetting(
      origin,
      origin,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      NO_RESOURCE_IDENTIFIER);

  if (setting == CONTENT_SETTING_ALLOW)
    return WebKit::WebNotificationPresenter::PermissionAllowed;
  if (setting == CONTENT_SETTING_BLOCK)
    return WebKit::WebNotificationPresenter::PermissionDenied;
  if (setting == CONTENT_SETTING_ASK)
    return WebKit::WebNotificationPresenter::PermissionNotAllowed;
  NOTREACHED() << "Invalid notifications settings value: " << setting;
  return WebKit::WebNotificationPresenter::PermissionNotAllowed;
}
