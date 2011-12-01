// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/browser/geolocation/geolocation_provider.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;

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

  // Called by the InfoBarDelegate to notify it's closed. It'll display a new
  // InfoBar if there's any request pending for this tab.
  void OnInfoBarClosed(int render_process_id,
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
                       const content::NotificationDetails& details);

 private:
  struct PendingInfoBarRequest;
  class RequestEquals;

  typedef std::vector<PendingInfoBarRequest> PendingInfoBarRequests;

  // Shows the first pending infobar for this tab.
  void ShowQueuedInfoBar(int render_process_id, int render_view_id);

  // Cancels an InfoBar request and returns the next iterator position.
  PendingInfoBarRequests::iterator CancelInfoBarRequestInternal(
      PendingInfoBarRequests::iterator i);

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

 private:
  virtual ~GeolocationConfirmInfoBarDelegate();

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
  // The unique id of the committed NavigationEntry of the TabContents that we
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
      infobar_helper->tab_contents()->controller().GetLastCommittedEntry();
  committed_contents_unique_id_ = committed_entry ?
      committed_entry->unique_id() : 0;
}

GeolocationConfirmInfoBarDelegate::~GeolocationConfirmInfoBarDelegate() {
  controller_->OnInfoBarClosed(render_process_id_, render_view_id_,
                               bridge_id_);
}

bool GeolocationConfirmInfoBarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  if (details.did_replace_entry || !details.is_navigation_to_different_page())
    return false;
  return committed_contents_unique_id_ != details.entry->unique_id() ||
      content::PageTransitionStripQualifier(details.entry->transition_type()) ==
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
      requesting_frame_url_, owner()->tab_contents()->GetURL(), true);
  return true;
}

bool GeolocationConfirmInfoBarDelegate::Cancel() {
  controller_->OnPermissionSet(render_process_id_, render_view_id_, bridge_id_,
      requesting_frame_url_, owner()->tab_contents()->GetURL(), false);
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

  owner()->tab_contents()->OpenURL(
      google_util::AppendGoogleLocaleParam(GURL(kGeolocationLearnMoreUrl)),
      GURL(), (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK);
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
  InfoBarDelegate* infobar_delegate;
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

  pending_infobar_requests_.push_back(PendingInfoBarRequest(render_process_id,
      render_view_id, bridge_id, requesting_frame, embedder, callback));
  ShowQueuedInfoBar(render_process_id, render_view_id);
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
  if (i != pending_infobar_requests_.end())
    CancelInfoBarRequestInternal(i);
}

void GeolocationInfoBarQueueController::OnInfoBarClosed(int render_process_id,
                                                        int render_view_id,
                                                        int bridge_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PendingInfoBarRequests::iterator i = std::find_if(
      pending_infobar_requests_.begin(), pending_infobar_requests_.end(),
      RequestEquals(render_process_id, render_view_id, bridge_id));
  if (i != pending_infobar_requests_.end())
    pending_infobar_requests_.erase(i);

  ShowQueuedInfoBar(render_process_id, render_view_id);
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
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();

  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ) {
    if (i->IsForPair(requesting_frame, embedder)) {
      // Cancel this request first, then notify listeners.  TODO(pkasting): Why
      // is this order important?
      // NOTE: If the pending request had an infobar, TabContents will close it
      // either synchronously or asynchronously, which will then pump the queue
      // via OnInfoBarClosed().
      PendingInfoBarRequest copied_request = *i;
      // Don't let CancelInfoBarRequestInternal() call RemoveInfoBar() with the
      // delegate that's currently calling us.  That delegate is in either
      // Accept() or Cancel(), so its owning InfoBar will call RemoveInfoBar()
      // later on in this callstack anyway; and if we do it here, and it causes
      // the delegate to be deleted, our GURL& args will point to garbage and we
      // may also cause other problems during stack unwinding.
      if (i->Equals(render_process_id, render_view_id, bridge_id))
        i->infobar_delegate = NULL;
      i = CancelInfoBarRequestInternal(i);

      geolocation_permission_context_->NotifyPermissionSet(
          copied_request.render_process_id, copied_request.render_view_id,
          copied_request.bridge_id, copied_request.requesting_frame,
          copied_request.callback, allowed);
    } else {
      ++i;
    }
  }
}

void GeolocationInfoBarQueueController::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  registrar_.Remove(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                    source);
  TabContents* tab_contents = content::Source<TabContents>(source).ptr();
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end();) {
    if (i->infobar_delegate == NULL &&
        tab_contents == tab_util::GetTabContentsByID(i->render_process_id,
                                                     i->render_view_id)) {
      i = pending_infobar_requests_.erase(i);
    } else {
      ++i;
    }
  }
}

void GeolocationInfoBarQueueController::ShowQueuedInfoBar(int render_process_id,
                                                          int render_view_id) {
  TabContents* tab_contents =
      tab_util::GetTabContentsByID(render_process_id, render_view_id);
  TabContentsWrapper* wrapper = NULL;
  if (tab_contents)
    wrapper = TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ) {
    if (i->IsForTab(render_process_id, render_view_id)) {
      if (!wrapper) {
        i = pending_infobar_requests_.erase(i);
        continue;
      }

      if (!i->infobar_delegate) {
        if (!registrar_.IsRegistered(
                this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                content::Source<TabContents>(tab_contents))) {
          registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                         content::Source<TabContents>(tab_contents));
        }
        i->infobar_delegate = new GeolocationConfirmInfoBarDelegate(
            wrapper->infobar_tab_helper(), this, render_process_id,
            render_view_id, i->bridge_id, i->requesting_frame,
            profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
        wrapper->infobar_tab_helper()->AddInfoBar(i->infobar_delegate);
      }
      break;
    }
    ++i;
  }
}

GeolocationInfoBarQueueController::PendingInfoBarRequests::iterator
    GeolocationInfoBarQueueController::CancelInfoBarRequestInternal(
    PendingInfoBarRequests::iterator i) {
  InfoBarDelegate* delegate = i->infobar_delegate;
  if (!delegate)
    return pending_infobar_requests_.erase(i);

  TabContents* tab_contents =
      tab_util::GetTabContentsByID(i->render_process_id, i->render_view_id);
  if (!tab_contents)
    return pending_infobar_requests_.erase(i);

  // TabContents will destroy the InfoBar, which will remove from our vector
  // asynchronously.
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  wrapper->infobar_tab_helper()->RemoveInfoBar(i->infobar_delegate);
  return ++i;
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

  ExtensionService* extensions = profile_->GetExtensionService();
  if (extensions) {
    const Extension* ext = extensions->GetExtensionByURL(requesting_frame);
    if (!ext)
      ext = extensions->GetExtensionByWebExtent(requesting_frame);
    if (ext && ext->HasAPIPermission(ExtensionAPIPermission::kGeolocation)) {
      // Make sure the extension is in the calling process.
      if (extensions->process_map()->Contains(ext->id(), render_process_id)) {
        NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                            requesting_frame, callback, true);
        return;
      }
    }
  }

  TabContents* tab_contents =
      tab_util::GetTabContentsByID(render_process_id, render_view_id);
  if (!tab_contents ||
      tab_contents->GetRenderViewType() != content::VIEW_TYPE_TAB_CONTENTS) {
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

  GURL embedder = tab_contents->GetURL();
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

  // TabContents may have gone away (or not exists for extension).
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::Get(render_process_id, render_view_id);
  if (content_settings) {
    content_settings->OnGeolocationPermissionSet(requesting_frame.GetOrigin(),
                                                 allowed);
  }

  callback.Run(allowed);

  if (allowed) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &ChromeGeolocationPermissionContext::
                NotifyArbitratorPermissionGranted, this, requesting_frame));
  }
}

void ChromeGeolocationPermissionContext::NotifyArbitratorPermissionGranted(
    const GURL& requesting_frame) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GeolocationProvider::GetInstance()->OnPermissionGranted(requesting_frame);
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
