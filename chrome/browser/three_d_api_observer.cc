// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/three_d_api_observer.h"

#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/three_d_api_types.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ThreeDAPIObserver)

// ThreeDAPIInfoBarDelegate ---------------------------------------------

// TODO(kbr): write a "learn more" article about the issues associated
// with client 3D APIs and GPU resets, and override GetLinkText(), etc.

class ThreeDAPIInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  ThreeDAPIInfoBarDelegate(
      InfoBarService* owner,
      const GURL& url,
      content::ThreeDAPIType requester);

 private:
  virtual ~ThreeDAPIInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const OVERRIDE;
  virtual ThreeDAPIInfoBarDelegate* AsThreeDAPIInfoBarDelegate() OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  GURL url_;
  content::ThreeDAPIType requester_;

  DISALLOW_COPY_AND_ASSIGN(ThreeDAPIInfoBarDelegate);
};

ThreeDAPIInfoBarDelegate::ThreeDAPIInfoBarDelegate(
    InfoBarService* owner,
    const GURL& url,
    content::ThreeDAPIType requester)
    : ConfirmInfoBarDelegate(owner),
      url_(url),
      requester_(requester) {
}

ThreeDAPIInfoBarDelegate::~ThreeDAPIInfoBarDelegate() {
}

bool ThreeDAPIInfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  ThreeDAPIInfoBarDelegate* three_d_delegate =
      delegate->AsThreeDAPIInfoBarDelegate();
  // For the time being, if a given web page is actually using both
  // WebGL and Pepper 3D and both APIs are blocked, just leave the
  // first infobar up. If the user selects "try again", both APIs will
  // be unblocked and the web page reload will succeed.
  return (three_d_delegate != NULL);
}

ThreeDAPIInfoBarDelegate*
ThreeDAPIInfoBarDelegate::AsThreeDAPIInfoBarDelegate() {
  return this;
}

string16 ThreeDAPIInfoBarDelegate::GetMessageText() const {
  string16 api_name;
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

string16 ThreeDAPIInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_3D_APIS_BLOCKED_OK_BUTTON_LABEL :
      IDS_3D_APIS_BLOCKED_TRY_AGAIN_BUTTON_LABEL);
}

bool ThreeDAPIInfoBarDelegate::Accept() {
  // TODO(kbr): add UMA stats.
  return true;
}

bool ThreeDAPIInfoBarDelegate::Cancel() {
  // TODO(kbr): add UMA stats.
  content::GpuDataManager::GetInstance()->UnblockDomainFrom3DAPIs(url_);
  owner()->GetWebContents()->GetController().Reload(true);
  return true;
}


// ThreeDAPIObserver ----------------------------------------------------

ThreeDAPIObserver::ThreeDAPIObserver(content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

ThreeDAPIObserver::~ThreeDAPIObserver() {}

void ThreeDAPIObserver::DidBlock3DAPIs(const GURL& url,
                                       content::ThreeDAPIType requester) {
  InfoBarService* service = InfoBarService::FromWebContents(web_contents());
  service->AddInfoBar(new ThreeDAPIInfoBarDelegate(service, url, requester));
}
