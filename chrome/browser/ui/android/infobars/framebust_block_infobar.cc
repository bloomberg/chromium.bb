// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/framebust_block_infobar.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/interventions/framebust_block_message_delegate_bridge.h"
#include "chrome/browser/ui/interventions/framebust_block_message_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "jni/FramebustBlockInfoBar_jni.h"

namespace {
class FramebustBlockInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return infobars::InfoBarDelegate::InfoBarIdentifier::
        FRAMEBUST_BLOCK_INFOBAR_ANDROID;
  }

  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override {
    return delegate->GetIdentifier() == GetIdentifier();
  }
};

}  // namespace

FramebustBlockInfoBar::FramebustBlockInfoBar(
    std::unique_ptr<FramebustBlockMessageDelegate> delegate)
    : InfoBarAndroid(base::MakeUnique<FramebustBlockInfoBarDelegate>()),
      delegate_bridge_(base::MakeUnique<FramebustBlockMessageDelegateBridge>(
          std::move(delegate))) {}

FramebustBlockInfoBar::~FramebustBlockInfoBar() = default;

void FramebustBlockInfoBar::ProcessButton(int action) {
  // Closes the infobar from the Java UI code.
  NOTREACHED();
}

base::android::ScopedJavaLocalRef<jobject>
FramebustBlockInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  return Java_FramebustBlockInfoBar_create(
      env, reinterpret_cast<uintptr_t>(delegate_bridge_.get()));
}

// static
void FramebustBlockInfoBar::Show(
    content::WebContents* web_contents,
    std::unique_ptr<FramebustBlockMessageDelegate> message_delegate) {
  InfoBarService* service = InfoBarService::FromWebContents(web_contents);
  service->AddInfoBar(
      base::WrapUnique(new FramebustBlockInfoBar(std::move(message_delegate))),
      /*replace_existing=*/true);
}
