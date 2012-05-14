// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"

#include <functional>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_view_type.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using WebKit::WebSecurityOrigin;
using content::BrowserThread;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

// GeolocationInfoBarQueueController ------------------------------------------

// This class controls the geolocation infobar queue per profile, and it's an
// internal class to GeolocationPermissionContext.
// An alternate approach would be to have this queue per tab, and use
// notifications to broadcast when permission is set / listen to notification to
// cancel pending requests. This may be specially useful if there are other
// things listening for such notifications.
// For the time being this class is self-contained and it doesn't seem pulling
// the notification infrastructure would simplify.
class GeolocationInfoBarQueueController : content::NotificationObserver {
 public:
  GeolocationInfoBarQueueController(
      ChromeGeolocationPermissionContext* geolocation_permission_context,
      Profile* profile);
  ~GeolocationInfoBarQueueController();

  // The InfoBar will be displayed immediately if the tab is not already
  // displaying one, otherwise it'll be queued.
  void CreateInfoBarRequest(int render_process_id,
                            int render_view_id,
                            int bridge_id,
                            const GURL& requesting_frame,
                            const GURL& emebedder,
                            base::Callback<void(bool)> callback);

  // Cancels a specific infobar request.
  void CancelInfoBarRequest(int render_process_id,
                            int render_view_id,
                            int bridge_id);

  // Called by the InfoBarDelegate to notify permission has been set.
  // It'll notify and dismiss any other pending InfoBar request for the same
  // |requesting_frame| and embedder.
  void OnPermissionSet(int render_process_id,
                       int render_view_id,
                       int bridge_id,
                       const GURL& requesting_frame,
                       const GURL& embedder,
                       bool allowed);

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  struct PendingInfoBarRequest;
  class RequestEquals;

  typedef std::vector<PendingInfoBarRequest> PendingInfoBarRequests;

  // Shows the first pending infobar for this tab.
  void ShowQueuedInfoBar(int render_process_id, int render_view_id,
                         InfoBarTabHelper* helper);

  // Removes any pending requests for a given tab.
  void ClearPendingInfoBarRequestsForTab(int render_process_id,
                                         int render_view_id);

  InfoBarTabHelper* GetInfoBarHelper(int render_process_id, int render_view_id);
  bool AlreadyShowingInfoBar(int render_process_id, int render_view_id);

  content::NotificationRegistrar registrar_;

  ChromeGeolocationPermissionContext* const geolocation_permission_context_;
  Profile* const profile_;
  PendingInfoBarRequests pending_infobar_requests_;
};


// GeolocationConfirmInfoBarDelegate ------------------------------------------

namespace {

class GeolocationConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  GeolocationConfirmInfoBarDelegate(
      InfoBarTabHelper* infobar_helper,
      GeolocationInfoBarQueueController* controller,
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame_url,
      const std::string& display_languages);

  int render_process_id() const { return render_process_id_; }
  int render_view_id() const { return render_view_id_; }

 private:

  // ConfirmInfoBarDelegate:
  virtual bool ShouldExpire(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  GeolocationInfoBarQueueController* controller_;
  int render_process_id_;
  int render_view_id_;
  int bridge_id_;
  // The unique id of the committed NavigationEntry of the WebContents that we
  // were opened for. Used to help expire on navigations.
  int committed_contents_unique_id_;

  GURL requesting_frame_url_;
  std::string display_languages_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GeolocationConfirmInfoBarDelegate);
};

GeolocationConfirmInfoBarDelegate::GeolocationConfirmInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    GeolocationInfoBarQueueController* controller,
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame_url,
    const std::string& display_languages)
    : ConfirmInfoBarDelegate(infobar_helper),
      controller_(controller),
      render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      bridge_id_(bridge_id),
      requesting_frame_url_(requesting_frame_url),
      display_languages_(display_languages) {
  const NavigationEntry* committed_entry =
      infobar_helper->web_contents()->GetController().GetLastCommittedEntry();
  committed_contents_unique_id_ = committed_entry ?
      committed_entry->GetUniqueID() : 0;
}

bool GeolocationConfirmInfoBarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  if (details.did_replace_entry || !details.is_navigation_to_different_page())
    return false;
  return committed_contents_unique_id_ != details.entry->GetUniqueID() ||
      content::PageTransitionStripQualifier(
          details.entry->GetTransitionType()) ==
              content::PAGE_TRANSITION_RELOAD;
}

gfx::Image* GeolocationConfirmInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_GEOLOCATION_INFOBAR_ICON);
}

InfoBarDelegate::Type
    GeolocationConfirmInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 GeolocationConfirmInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_GEOLOCATION_INFOBAR_QUESTION,
      net::FormatUrl(requesting_frame_url_.GetOrigin(), display_languages_));
}

string16 GeolocationConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_GEOLOCATION_ALLOW_BUTTON : IDS_GEOLOCATION_DENY_BUTTON);
}

bool GeolocationConfirmInfoBarDelegate::Accept() {
  controller_->OnPermissionSet(render_process_id_, render_view_id_, bridge_id_,
      requesting_frame_url_, owner()->web_contents()->GetURL(), true);
  return true;
}

bool GeolocationConfirmInfoBarDelegate::Cancel() {
  controller_->OnPermissionSet(render_process_id_, render_view_id_, bridge_id_,
      requesting_frame_url_, owner()->web_contents()->GetURL(),
      false);
  return true;
}

string16 GeolocationConfirmInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool GeolocationConfirmInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  const char kGeolocationLearnMoreUrl[] =
#if defined(OS_CHROMEOS)
      "https://www.google.com/support/chromeos/bin/answer.py?answer=142065";
#else
      "https://www.google.com/support/chrome/bin/answer.py?answer=142065";
#endif

  OpenURLParams params(
      google_util::AppendGoogleLocaleParam(GURL(kGeolocationLearnMoreUrl)),
      Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK, false);
  owner()->web_contents()->OpenURL(params);
  return false;  // Do not dismiss the info bar.
}

}  // namespace


// GeolocationInfoBarQueueController::PendingInfoBarRequest -------------------

struct GeolocationInfoBarQueueController::PendingInfoBarRequest {
 public:
  PendingInfoBarRequest(int render_process_id,
                        int render_view_id,
                        int bridge_id,
                        const GURL& requesting_frame,
                        const GURL& embedder,
                        base::Callback<void(bool)> callback);

  bool IsForTab(int p_render_process_id, int p_render_view_id) const;
  bool IsForPair(const GURL& p_requesting_frame,
                 const GURL& p_embedder) const;
  bool Equals(int p_render_process_id,
              int p_render_view_id,
              int p_bridge_id) const;

  int render_process_id;
  int render_view_id;
  int bridge_id;
  GURL requesting_frame;
  GURL embedder;
  base::Callback<void(bool)> callback;
  GeolocationConfirmInfoBarDelegate* infobar_delegate;
};

GeolocationInfoBarQueueController::PendingInfoBarRequest::PendingInfoBarRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    const GURL& embedder,
    base::Callback<void(bool)> callback)
    : render_process_id(render_process_id),
      render_view_id(render_view_id),
      bridge_id(bridge_id),
      requesting_frame(requesting_frame),
      embedder(embedder),
      callback(callback),
      infobar_delegate(NULL) {
}

bool GeolocationInfoBarQueueController::PendingInfoBarRequest::IsForTab(
    int p_render_process_id,
    int p_render_view_id) const {
  return (render_process_id == p_render_process_id) &&
      (render_view_id == p_render_view_id);
}

bool GeolocationInfoBarQueueController::PendingInfoBarRequest::IsForPair(
    const GURL& p_requesting_frame,
    const GURL& p_embedder) const {
  return (requesting_frame == p_requesting_frame) && (embedder == p_embedder);
}

bool GeolocationInfoBarQueueController::PendingInfoBarRequest::Equals(
    int p_render_process_id,
    int p_render_view_id,
    int p_bridge_id) const {
  return IsForTab(p_render_process_id, p_render_view_id) &&
     (bridge_id == p_bridge_id);
}


// GeolocationInfoBarQueueController::RequestEquals ---------------------------

// Useful predicate for checking PendingInfoBarRequest equality.
class GeolocationInfoBarQueueController::RequestEquals
    : public std::unary_function<PendingInfoBarRequest, bool> {
 public:
  RequestEquals(int render_process_id, int render_view_id, int bridge_id);

  bool operator()(const PendingInfoBarRequest& request) const;

 private:
  int render_process_id_;
  int render_view_id_;
  int bridge_id_;
};

GeolocationInfoBarQueueController::RequestEquals::RequestEquals(
    int render_process_id,
    int render_view_id,
    int bridge_id)
    : render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      bridge_id_(bridge_id) {
}

bool GeolocationInfoBarQueueController::RequestEquals::operator()(
    const PendingInfoBarRequest& request) const {
  return request.Equals(render_process_id_, render_view_id_, bridge_id_);
}

// GeolocationInfoBarQueueController ------------------------------------------

GeolocationInfoBarQueueController::GeolocationInfoBarQueueController(
    ChromeGeolocationPermissionContext* geolocation_permission_context,
    Profile* profile)
    : geolocation_permission_context_(geolocation_permission_context),
      profile_(profile) {
}

GeolocationInfoBarQueueController::~GeolocationInfoBarQueueController() {
}

void GeolocationInfoBarQueueController::CreateInfoBarRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    const GURL& embedder,
    base::Callback<void(bool)> callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We shouldn't get duplicate requests.
  DCHECK(std::find_if(pending_infobar_requests_.begin(),
      pending_infobar_requests_.end(),
      RequestEquals(render_process_id, render_view_id, bridge_id)) ==
      pending_infobar_requests_.end());

  InfoBarTabHelper* helper = GetInfoBarHelper(render_process_id,
                                              render_view_id);
  if (!helper) {
    // We won't be able to create any infobars, shortcut and remove any pending
    // requests.
    ClearPendingInfoBarRequestsForTab(render_process_id, render_view_id);
    return;
  }
  pending_infobar_requests_.push_back(PendingInfoBarRequest(render_process_id,
      render_view_id, bridge_id, requesting_frame, embedder, callback));
  if (!AlreadyShowingInfoBar(render_process_id, render_view_id))
    ShowQueuedInfoBar(render_process_id, render_view_id, helper);
}

void GeolocationInfoBarQueueController::CancelInfoBarRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PendingInfoBarRequests::iterator i = std::find_if(
      pending_infobar_requests_.begin(), pending_infobar_requests_.end(),
      RequestEquals(render_process_id, render_view_id, bridge_id));
  // TODO(pkasting): Can this conditional become a DCHECK()?
  if (i == pending_infobar_requests_.end())
    return;
  InfoBarDelegate* delegate = i->infobar_delegate;
  if (!delegate) {
    pending_infobar_requests_.erase(i);
    return;
  }
  InfoBarTabHelper* helper = GetInfoBarHelper(render_process_id,
                                              render_view_id);
  helper->RemoveInfoBar(delegate);
}

void GeolocationInfoBarQueueController::OnPermissionSet(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    const GURL& embedder,
    bool allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ContentSetting content_setting =
      allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  profile_->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(requesting_frame.GetOrigin()),
      ContentSettingsPattern::FromURLNoWildcard(embedder.GetOrigin()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      content_setting);

  // Cancel this request first, then notify listeners.  TODO(pkasting): Why
  // is this order important?
  PendingInfoBarRequests requests_to_notify;
  PendingInfoBarRequests infobars_to_remove;
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ) {
    if (i->IsForPair(requesting_frame, embedder)) {
      requests_to_notify.push_back(*i);
      if (i->Equals(render_process_id, render_view_id, bridge_id)) {
        // The delegate that called us is i, and it's currently in either
        // Accept() or Cancel(). This means that the owning InfoBar will call
        // RemoveInfoBar() later on, and that will trigger a notification we're
        // observing.
        ++i;
      } else if (i->infobar_delegate) {
        // This InfoBar is for the same frame/embedder pair, but in a different
        // tab. We should remove it now that we've got an answer for it.
        infobars_to_remove.push_back(*i);
        ++i;
      } else {
        // We haven't created an InfoBar yet, just remove the pending request.
        i = pending_infobar_requests_.erase(i);
      }
    } else {
      ++i;
    }
  }

  // Remove all InfoBars for the same |requesting_frame| and |embedder|.
  for (PendingInfoBarRequests::iterator i = infobars_to_remove.begin();
       i != infobars_to_remove.end(); ++i ) {
    InfoBarTabHelper* helper = GetInfoBarHelper(i->render_process_id,
                                                i->render_view_id);
    helper->RemoveInfoBar(i->infobar_delegate);
  }

  // Send out the permission notifications.
  for (PendingInfoBarRequests::iterator i = requests_to_notify.begin();
       i != requests_to_notify.end(); ++i ) {
    geolocation_permission_context_->NotifyPermissionSet(
        i->render_process_id, i->render_view_id,
        i->bridge_id, i->requesting_frame,
        i->callback, allowed);
  }
}

void GeolocationInfoBarQueueController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED, type);
  // We will receive this notification for all infobar closures, so we need to
  // check whether this is the geolocation infobar we're tracking. Note that the
  // InfoBarContainer (if any) may have received this notification before us and
  // caused the delegate to be deleted, so it's not safe to dereference the
  // contents of the delegate. The address of the delegate, however, is OK to
  // use to find the PendingInfoBarRequest to remove because
  // pending_infobar_requests_ will not have received any new entries between
  // the NotificationService's call to InfoBarContainer::Observe and this
  // method.
  InfoBarDelegate* delegate =
      content::Details<InfoBarRemovedDetails>(details)->first;
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    GeolocationConfirmInfoBarDelegate* confirm_delegate = i->infobar_delegate;
    if (confirm_delegate == delegate) {
      InfoBarTabHelper* helper =
          content::Source<InfoBarTabHelper>(source).ptr();
      registrar_.Remove(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                        source);
      int render_process_id = i->render_process_id;
      int render_view_id = i->render_view_id;
      pending_infobar_requests_.erase(i);
      ShowQueuedInfoBar(render_process_id, render_view_id, helper);
      return;
    }
  }
}

void GeolocationInfoBarQueueController::ClearPendingInfoBarRequestsForTab(
    int render_process_id,
    int render_view_id) {
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ) {
    if (i->IsForTab(render_process_id, render_view_id))
      i = pending_infobar_requests_.erase(i);
    else
      ++i;
  }
}

InfoBarTabHelper* GeolocationInfoBarQueueController::GetInfoBarHelper(
    int render_process_id,
    int render_view_id) {
  WebContents* tab_contents =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);
  if (!tab_contents)
    return NULL;
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  if (!wrapper)
    return NULL;
  return wrapper->infobar_tab_helper();
}

bool GeolocationInfoBarQueueController::AlreadyShowingInfoBar(
    int render_process_id,
    int render_view_id) {
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (i->IsForTab(render_process_id, render_view_id) && i->infobar_delegate)
      return true;
  }
  return false;
}

void GeolocationInfoBarQueueController::ShowQueuedInfoBar(
    int render_process_id,
    int render_view_id,
    InfoBarTabHelper* helper) {
  DCHECK(helper);
  DCHECK(!AlreadyShowingInfoBar(render_process_id, render_view_id));
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (i->IsForTab(render_process_id, render_view_id) &&
        !i->infobar_delegate) {
        DCHECK(!registrar_.IsRegistered(
            this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
            content::Source<InfoBarTabHelper>(helper)));
        registrar_.Add(
            this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
            content::Source<InfoBarTabHelper>(helper));
        i->infobar_delegate = new GeolocationConfirmInfoBarDelegate(
            helper, this, render_process_id,
            render_view_id, i->bridge_id, i->requesting_frame,
            profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
        helper->AddInfoBar(i->infobar_delegate);
        break;
    }
  }
}

// GeolocationPermissionContext -----------------------------------------------

ChromeGeolocationPermissionContext::ChromeGeolocationPermissionContext(
    Profile* profile)
    : profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(geolocation_infobar_queue_controller_(
         new GeolocationInfoBarQueueController(this, profile))) {
}

ChromeGeolocationPermissionContext::~ChromeGeolocationPermissionContext() {
}

void ChromeGeolocationPermissionContext::RequestGeolocationPermission(
    int render_process_id, int render_view_id, int bridge_id,
    const GURL& requesting_frame, base::Callback<void(bool)> callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &ChromeGeolocationPermissionContext::RequestGeolocationPermission,
            this, render_process_id, render_view_id, bridge_id,
            requesting_frame, callback));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ExtensionService* extension_service = profile_->GetExtensionService();
  if (extension_service) {
    const Extension* extension =
        extension_service->extensions()->GetExtensionOrAppByURL(
            ExtensionURLInfo(
                WebSecurityOrigin::createFromString(
                    UTF8ToUTF16(requesting_frame.spec())),
                requesting_frame));
    if (extension &&
        extension->HasAPIPermission(ExtensionAPIPermission::kGeolocation)) {
      // Make sure the extension is in the calling process.
      if (extension_service->process_map()->Contains(
              extension->id(), render_process_id)) {
        NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                            requesting_frame, callback, true);
        return;
      }
    }
  }

  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);
  if (!web_contents || web_contents->GetViewType() !=
          chrome::VIEW_TYPE_TAB_CONTENTS) {
    // The tab may have gone away, or the request may not be from a tab at all.
    // TODO(mpcomplete): the request could be from a background page or
    // extension popup (tab_contents will have a different ViewType). But why do
    // we care? Shouldn't we still put an infobar up in the current tab?
    LOG(WARNING) << "Attempt to use geolocation tabless renderer: "
                 << render_process_id << "," << render_view_id << ","
                 << bridge_id << " (can't prompt user without a visible tab)";
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                        requesting_frame, callback, false);
    return;
  }

  GURL embedder = web_contents->GetURL();
  if (!requesting_frame.is_valid() || !embedder.is_valid()) {
    LOG(WARNING) << "Attempt to use geolocation from an invalid URL: "
                 << requesting_frame << "," << embedder
                 << " (geolocation is not supported in popups)";
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                        requesting_frame, callback, false);
    return;
  }

  ContentSetting content_setting =
     profile_->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame,
          embedder,
          CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string());
  if (content_setting == CONTENT_SETTING_BLOCK) {
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                        requesting_frame, callback, false);
  } else if (content_setting == CONTENT_SETTING_ALLOW) {
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                        requesting_frame, callback, true);
  } else {  // setting == ask. Prompt the user.
    geolocation_infobar_queue_controller_->CreateInfoBarRequest(
        render_process_id, render_view_id, bridge_id, requesting_frame,
        embedder, callback);
  }
}

void ChromeGeolocationPermissionContext::CancelGeolocationPermissionRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  CancelPendingInfoBarRequest(render_process_id, render_view_id, bridge_id);
}

void ChromeGeolocationPermissionContext::NotifyPermissionSet(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    base::Callback<void(bool)> callback,
    bool allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // WebContents may have gone away (or not exists for extension).
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::Get(render_process_id, render_view_id);
  if (content_settings) {
    content_settings->OnGeolocationPermissionSet(requesting_frame.GetOrigin(),
                                                 allowed);
  }

  callback.Run(allowed);
}

void ChromeGeolocationPermissionContext::CancelPendingInfoBarRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &ChromeGeolocationPermissionContext::CancelPendingInfoBarRequest,
            this, render_process_id, render_view_id, bridge_id));
     return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  geolocation_infobar_queue_controller_->CancelInfoBarRequest(render_process_id,
      render_view_id, bridge_id);
}
