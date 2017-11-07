// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/near_oom_infobar.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "jni/NearOomInfoBar_jni.h"

namespace {

class NearOomInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  explicit NearOomInfoBarDelegate(base::OnceClosure dismiss_callback)
      : dismiss_callback_(std::move(dismiss_callback)) {}

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return infobars::InfoBarDelegate::InfoBarIdentifier::
        NEAR_OOM_INFOBAR_ANDROID;
  }

  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override {
    return delegate->GetIdentifier() == GetIdentifier();
  }

  void InfoBarDismissed() override { std::move(dismiss_callback_).Run(); }

  base::OnceClosure dismiss_callback_;
};

}  // namespace

NearOomInfoBar::NearOomInfoBar(NearOomMessageDelegate* delegate)
    : InfoBarAndroid(std::make_unique<NearOomInfoBarDelegate>(
          base::BindOnce(&NearOomInfoBar::AcceptIntervention,
                         base::Unretained(this)))),
      delegate_(delegate) {
  DCHECK(delegate_);
}

NearOomInfoBar::~NearOomInfoBar() = default;

void NearOomInfoBar::AcceptIntervention() {
  delegate_->AcceptIntervention();
  DLOG(WARNING) << "Near-OOM Intervention accepted.";
}

void NearOomInfoBar::DeclineIntervention() {
  delegate_->DeclineIntervention();
  DLOG(WARNING) << "Near-OOM Intervention declined.";
}

void NearOomInfoBar::OnLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  DeclineIntervention();
  RemoveSelf();
}

void NearOomInfoBar::ProcessButton(int action) {
  NOTREACHED();  // No button on this infobar.
}

base::android::ScopedJavaLocalRef<jobject> NearOomInfoBar::CreateRenderInfoBar(
    JNIEnv* env) {
  return Java_NearOomInfoBar_create(env);
}

// static
void NearOomInfoBar::Show(content::WebContents* web_contents,
                          NearOomMessageDelegate* delegate) {
  InfoBarService* service = InfoBarService::FromWebContents(web_contents);
  service->AddInfoBar(base::WrapUnique(new NearOomInfoBar(delegate)));
}
