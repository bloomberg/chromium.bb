// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_quota_permission_context.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "storage/common/quota/quota_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#else
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#endif

namespace {

// If the site requested larger quota than this threshold, show a different
// message to the user.
const int64_t kRequestLargeQuotaThreshold = 5 * 1024 * 1024;

// QuotaPermissionRequest ---------------------------------------------

class QuotaPermissionRequest : public PermissionBubbleRequest {
 public:
  QuotaPermissionRequest(
      ChromeQuotaPermissionContext* context,
      const GURL& origin_url,
      int64_t requested_quota,
      bool user_gesture,
      const std::string& display_languages,
      const content::QuotaPermissionContext::PermissionCallback& callback);

  ~QuotaPermissionRequest() override;

  // PermissionBubbleRequest:
  int GetIconId() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetMessageTextFragment() const override;
  bool HasUserGesture() const override;
  GURL GetOrigin() const override;
  void PermissionGranted() override;
  void PermissionDenied() override;
  void Cancelled() override;
  void RequestFinished() override;

 private:
  scoped_refptr<ChromeQuotaPermissionContext> context_;
  GURL origin_url_;
  std::string display_languages_;
  int64_t requested_quota_;
  bool user_gesture_;
  content::QuotaPermissionContext::PermissionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(QuotaPermissionRequest);
};

QuotaPermissionRequest::QuotaPermissionRequest(
    ChromeQuotaPermissionContext* context,
    const GURL& origin_url,
    int64_t requested_quota,
    bool user_gesture,
    const std::string& display_languages,
    const content::QuotaPermissionContext::PermissionCallback& callback)
    : context_(context),
      origin_url_(origin_url),
      display_languages_(display_languages),
      requested_quota_(requested_quota),
      user_gesture_(user_gesture),
      callback_(callback) {}

QuotaPermissionRequest::~QuotaPermissionRequest() {}

int QuotaPermissionRequest::GetIconId() const {
  // TODO(gbillock): get the proper image here
  return IDR_INFOBAR_WARNING;
}

base::string16 QuotaPermissionRequest::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      (requested_quota_ > kRequestLargeQuotaThreshold
           ? IDS_REQUEST_LARGE_QUOTA_INFOBAR_QUESTION
           : IDS_REQUEST_QUOTA_INFOBAR_QUESTION),
      url_formatter::FormatUrlForSecurityDisplay(origin_url_,
                                                 display_languages_));
}

base::string16 QuotaPermissionRequest::GetMessageTextFragment() const {
  return l10n_util::GetStringUTF16(IDS_REQUEST_QUOTA_PERMISSION_FRAGMENT);
}

bool QuotaPermissionRequest::HasUserGesture() const {
  return user_gesture_;
}

GURL QuotaPermissionRequest::GetOrigin() const {
  return origin_url_;
}

void QuotaPermissionRequest::PermissionGranted() {
  context_->DispatchCallbackOnIOThread(
      callback_,
      content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW);
  callback_ = content::QuotaPermissionContext::PermissionCallback();
}

void QuotaPermissionRequest::PermissionDenied() {
  context_->DispatchCallbackOnIOThread(
      callback_,
      content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_DISALLOW);
  callback_ = content::QuotaPermissionContext::PermissionCallback();
}

void QuotaPermissionRequest::Cancelled() {
}

void QuotaPermissionRequest::RequestFinished() {
  if (!callback_.is_null()) {
    context_->DispatchCallbackOnIOThread(
        callback_,
        content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_CANCELLED);
  }

  delete this;
}

#if defined(OS_ANDROID)
// RequestQuotaInfoBarDelegate ------------------------------------------------

class RequestQuotaInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a request quota infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(
      InfoBarService* infobar_service,
      ChromeQuotaPermissionContext* context,
      const GURL& origin_url,
      int64_t requested_quota,
      const std::string& display_languages,
      const content::QuotaPermissionContext::PermissionCallback& callback);

 private:
  RequestQuotaInfoBarDelegate(
      ChromeQuotaPermissionContext* context,
      const GURL& origin_url,
      int64_t requested_quota,
      const std::string& display_languages,
      const content::QuotaPermissionContext::PermissionCallback& callback);
  ~RequestQuotaInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  bool Accept() override;
  bool Cancel() override;

  scoped_refptr<ChromeQuotaPermissionContext> context_;
  GURL origin_url_;
  std::string display_languages_;
  int64_t requested_quota_;
  content::QuotaPermissionContext::PermissionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(RequestQuotaInfoBarDelegate);
};

// static
void RequestQuotaInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    ChromeQuotaPermissionContext* context,
    const GURL& origin_url,
    int64_t requested_quota,
    const std::string& display_languages,
    const content::QuotaPermissionContext::PermissionCallback& callback) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new RequestQuotaInfoBarDelegate(
          context, origin_url, requested_quota, display_languages, callback))));
}

RequestQuotaInfoBarDelegate::RequestQuotaInfoBarDelegate(
    ChromeQuotaPermissionContext* context,
    const GURL& origin_url,
    int64_t requested_quota,
    const std::string& display_languages,
    const content::QuotaPermissionContext::PermissionCallback& callback)
    : ConfirmInfoBarDelegate(),
      context_(context),
      origin_url_(origin_url),
      display_languages_(display_languages),
      requested_quota_(requested_quota),
      callback_(callback) {}

RequestQuotaInfoBarDelegate::~RequestQuotaInfoBarDelegate() {
  if (!callback_.is_null()) {
    context_->DispatchCallbackOnIOThread(
        callback_,
        content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_CANCELLED);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier
RequestQuotaInfoBarDelegate::GetIdentifier() const {
  return REQUEST_QUOTA_INFOBAR_DELEGATE;
}

base::string16 RequestQuotaInfoBarDelegate::GetMessageText() const {
  // If the site requested larger quota than this threshold, show a different
  // message to the user.
  return l10n_util::GetStringFUTF16(
      (requested_quota_ > kRequestLargeQuotaThreshold
           ? IDS_REQUEST_LARGE_QUOTA_INFOBAR_QUESTION
           : IDS_REQUEST_QUOTA_INFOBAR_QUESTION),
      url_formatter::FormatUrlForSecurityDisplay(origin_url_,
                                                 display_languages_));
}

bool RequestQuotaInfoBarDelegate::Accept() {
  context_->DispatchCallbackOnIOThread(
      callback_,
      content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW);
  return true;
}

bool RequestQuotaInfoBarDelegate::Cancel() {
  context_->DispatchCallbackOnIOThread(
      callback_,
      content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_CANCELLED);
  return true;
}
#endif

}  // namespace


// ChromeQuotaPermissionContext -----------------------------------------------

ChromeQuotaPermissionContext::ChromeQuotaPermissionContext() {
}

void ChromeQuotaPermissionContext::RequestQuotaPermission(
    const content::StorageQuotaParams& params,
    int render_process_id,
    const PermissionCallback& callback) {
  if (params.storage_type != storage::kStorageTypePersistent) {
    // For now we only support requesting quota with this interface
    // for Persistent storage type.
    callback.Run(QUOTA_PERMISSION_RESPONSE_DISALLOW);
    return;
  }

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&ChromeQuotaPermissionContext::RequestQuotaPermission, this,
                   params, render_process_id, callback));
    return;
  }

  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_id,
                                   params.render_view_id);
  if (!web_contents) {
    // The tab may have gone away or the request may not be from a tab.
    LOG(WARNING) << "Attempt to request quota tabless renderer: "
                 << render_process_id << "," << params.render_view_id;
    DispatchCallbackOnIOThread(callback, QUOTA_PERMISSION_RESPONSE_CANCELLED);
    return;
  }

#if defined(OS_ANDROID)
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  if (infobar_service) {
    RequestQuotaInfoBarDelegate::Create(
        infobar_service, this, params.origin_url, params.requested_size,
        Profile::FromBrowserContext(web_contents->GetBrowserContext())->
            GetPrefs()->GetString(prefs::kAcceptLanguages),
        callback);
    return;
  }
#else
  PermissionBubbleManager* bubble_manager =
      PermissionBubbleManager::FromWebContents(web_contents);
  if (bubble_manager) {
    bubble_manager->AddRequest(new QuotaPermissionRequest(
        this, params.origin_url, params.requested_size, params.user_gesture,
        Profile::FromBrowserContext(web_contents->GetBrowserContext())
            ->GetPrefs()
            ->GetString(prefs::kAcceptLanguages),
        callback));
    return;
  }
#endif

  // The tab has no UI service for presenting the permissions request.
  LOG(WARNING) << "Attempt to request quota from a background page: "
               << render_process_id << "," << params.render_view_id;
  DispatchCallbackOnIOThread(callback, QUOTA_PERMISSION_RESPONSE_CANCELLED);
}

void ChromeQuotaPermissionContext::DispatchCallbackOnIOThread(
    const PermissionCallback& callback,
    QuotaPermissionResponse response) {
  DCHECK_EQ(false, callback.is_null());

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&ChromeQuotaPermissionContext::DispatchCallbackOnIOThread,
                   this, callback, response));
    return;
  }

  callback.Run(response);
}

ChromeQuotaPermissionContext::~ChromeQuotaPermissionContext() {}
