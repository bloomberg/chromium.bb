// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu/three_d_api_observer.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/gpu_data_manager.h"
#include "ui/base/l10n/l10n_util.h"


// ThreeDAPIInfoBarDelegate ---------------------------------------------------

class ThreeDAPIInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a 3D API infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     const GURL& url,
                     content::ThreeDAPIType requester);

 private:
  enum DismissalHistogram {
    IGNORED,
    RELOADED,
    CLOSED_WITHOUT_ACTION,
    DISMISSAL_MAX
  };

  ThreeDAPIInfoBarDelegate(const GURL& url, content::ThreeDAPIType requester);
  ~ThreeDAPIInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;
  ThreeDAPIInfoBarDelegate* AsThreeDAPIInfoBarDelegate() override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  GURL url_;
  content::ThreeDAPIType requester_;
  // Basically indicates whether the infobar was displayed at all, or
  // was a temporary instance thrown away by the InfobarService.
  mutable bool message_text_queried_;
  bool action_taken_;

  DISALLOW_COPY_AND_ASSIGN(ThreeDAPIInfoBarDelegate);
};

// static
void ThreeDAPIInfoBarDelegate::Create(InfoBarService* infobar_service,
                                      const GURL& url,
                                      content::ThreeDAPIType requester) {
  if (!infobar_service)
    return;  // NULL for apps.
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new ThreeDAPIInfoBarDelegate(url, requester))));
}

ThreeDAPIInfoBarDelegate::ThreeDAPIInfoBarDelegate(
    const GURL& url,
    content::ThreeDAPIType requester)
    : ConfirmInfoBarDelegate(),
      url_(url),
      requester_(requester),
      message_text_queried_(false),
      action_taken_(false) {
}

ThreeDAPIInfoBarDelegate::~ThreeDAPIInfoBarDelegate() {
  if (message_text_queried_ && !action_taken_) {
    UMA_HISTOGRAM_ENUMERATION("GPU.ThreeDAPIInfoBarDismissal",
                              CLOSED_WITHOUT_ACTION, DISMISSAL_MAX);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier
ThreeDAPIInfoBarDelegate::GetIdentifier() const {
  return THREE_D_API_INFOBAR_DELEGATE;
}

int ThreeDAPIInfoBarDelegate::GetIconId() const {
  return IDR_INFOBAR_3D_BLOCKED;
}

bool ThreeDAPIInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  // For the time being, if a given web page is actually using both
  // WebGL and Pepper 3D and both APIs are blocked, just leave the
  // first infobar up. If the user selects "try again", both APIs will
  // be unblocked and the web page reload will succeed.
  return !!delegate->AsThreeDAPIInfoBarDelegate();
}

ThreeDAPIInfoBarDelegate*
    ThreeDAPIInfoBarDelegate::AsThreeDAPIInfoBarDelegate() {
  return this;
}

base::string16 ThreeDAPIInfoBarDelegate::GetMessageText() const {
  message_text_queried_ = true;

  base::string16 api_name;
  switch (requester_) {
    case content::THREE_D_API_TYPE_WEBGL:
      api_name = l10n_util::GetStringUTF16(IDS_3D_APIS_WEBGL_NAME);
      break;
    case content::THREE_D_API_TYPE_PEPPER_3D:
      api_name = l10n_util::GetStringUTF16(IDS_3D_APIS_PEPPER_3D_NAME);
      break;
  }

  return l10n_util::GetStringFUTF16(IDS_3D_APIS_BLOCKED_TEXT,
                                    api_name);
}

base::string16 ThreeDAPIInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_3D_APIS_BLOCKED_OK_BUTTON_LABEL :
      IDS_3D_APIS_BLOCKED_TRY_AGAIN_BUTTON_LABEL);
}

bool ThreeDAPIInfoBarDelegate::Accept() {
  action_taken_ = true;
  UMA_HISTOGRAM_ENUMERATION("GPU.ThreeDAPIInfoBarDismissal", IGNORED,
                            DISMISSAL_MAX);
  return true;
}

bool ThreeDAPIInfoBarDelegate::Cancel() {
  action_taken_ = true;
  UMA_HISTOGRAM_ENUMERATION("GPU.ThreeDAPIInfoBarDismissal", RELOADED,
                            DISMISSAL_MAX);
  content::GpuDataManager::GetInstance()->UnblockDomainFrom3DAPIs(url_);
  InfoBarService::WebContentsFromInfoBar(infobar())->GetController().Reload(
      true);
  return true;
}

// ThreeDAPIObserver ----------------------------------------------------------

ThreeDAPIObserver::ThreeDAPIObserver() {
  content::GpuDataManager::GetInstance()->AddObserver(this);
}

ThreeDAPIObserver::~ThreeDAPIObserver() {
  content::GpuDataManager::GetInstance()->RemoveObserver(this);
}

void ThreeDAPIObserver::DidBlock3DAPIs(const GURL& top_origin_url,
                                       int render_process_id,
                                       int render_frame_id,
                                       content::ThreeDAPIType requester) {
  content::WebContents* web_contents = tab_util::GetWebContentsByFrameID(
      render_process_id, render_frame_id);
  if (!web_contents)
    return;
  ThreeDAPIInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents), top_origin_url, requester);
}
