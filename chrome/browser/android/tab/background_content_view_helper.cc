// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab/background_content_view_helper.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/chrome_web_contents_delegate_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/common/variations/variations_util.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "jni/BackgroundContentViewHelper_jni.h"

BackgroundContentViewHelper* BackgroundContentViewHelper::FromJavaObject(
    JNIEnv* env,
    jobject jobj) {
  return reinterpret_cast<BackgroundContentViewHelper*>(
      Java_BackgroundContentViewHelper_getNativePtr(env, jobj));
}

BackgroundContentViewHelper::BackgroundContentViewHelper() {
}

BackgroundContentViewHelper::~BackgroundContentViewHelper() {
}

void BackgroundContentViewHelper::Destroy(JNIEnv* env, jobject jobj) {
  delete this;
}

void BackgroundContentViewHelper::SetWebContents(
    JNIEnv* env,
    jobject jobj,
    jobject jcontent_view_core,
    jobject jweb_contents_delegate) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::GetNativeContentViewCore(env,
                                                         jcontent_view_core);
  DCHECK(content_view_core);
  DCHECK(content_view_core->GetWebContents());

  web_contents_.reset(content_view_core->GetWebContents());
  // TODO(ksimbili): Do we actually need tab helpers?
  TabAndroid::AttachTabHelpers(web_contents_.get());
  WindowAndroidHelper::FromWebContents(web_contents_.get())->
      SetWindowAndroid(content_view_core->GetWindowAndroid());
  web_contents_delegate_.reset(
      new chrome::android::ChromeWebContentsDelegateAndroid(
          env, jweb_contents_delegate));
  web_contents_delegate_->LoadProgressChanged(web_contents_.get(), 0);
  web_contents_->SetDelegate(web_contents_delegate_.get());
}

void BackgroundContentViewHelper::ReleaseWebContents(JNIEnv* env,
                                                     jobject jobj) {
  DCHECK(web_contents_.get());
  ignore_result(web_contents_.release());
}

void BackgroundContentViewHelper::DestroyWebContents(JNIEnv* env,
                                                     jobject jobj) {
  DCHECK(web_contents_.get());
  web_contents_->SetDelegate(NULL);
  web_contents_.reset();
  DCHECK(web_contents_delegate_.get());
  web_contents_delegate_.reset();
}

void BackgroundContentViewHelper::MergeHistoryFrom(JNIEnv* env,
                                                   jobject obj,
                                                   jobject jweb_contents) {
  if (!web_contents_.get())
    return;
  content::WebContents* from_web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(from_web_contents);

  if (web_contents_->GetController().CanPruneAllButLastCommitted()) {
    web_contents_->GetController().CopyStateFromAndPrune(
        &from_web_contents->GetController(), true);
  } else {
    web_contents_->GetController().CopyStateFrom(
        from_web_contents->GetController());
  }
}

namespace {

class OnCloseWebContentsDeleter
     : public content::WebContentsDelegate,
       public base::SupportsWeakPtr<OnCloseWebContentsDeleter> {
  public:
   explicit OnCloseWebContentsDeleter(content::WebContents* web_contents)
       : web_contents_(web_contents) {
     web_contents_->SetDelegate(this);
     base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
         base::Bind(&OnCloseWebContentsDeleter::Destroy,
                    AsWeakPtr()),
         base::TimeDelta::FromSeconds(kDeleteWithExtremePrejudiceSeconds));
   }

   void CloseContents(content::WebContents* source) override {
     DCHECK_EQ(web_contents_, source);
     Destroy();
   }

   void SwappedOut(content::WebContents* source) override {
     DCHECK_EQ(web_contents_, source);
     Destroy();
   }

   virtual void Destroy() {
     delete this;
   }

  private:
   static const int kDeleteWithExtremePrejudiceSeconds = 3;

   scoped_ptr<content::WebContents> web_contents_;

   DISALLOW_COPY_AND_ASSIGN(OnCloseWebContentsDeleter);
 };

}  // namespace

void BackgroundContentViewHelper::UnloadAndDeleteWebContents(
    JNIEnv* env,
    jobject jobj,
    jobject jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents)
    return;
  web_contents->DispatchBeforeUnload(false);
  ignore_result(new OnCloseWebContentsDeleter(web_contents));
}

int BackgroundContentViewHelper::GetSwapTimeoutMs(JNIEnv* env, jobject jobj) {
  std::string value = variations::GetVariationParamValue(
      "InstantSearchClicks", "swap_timeout");
  int swap_timeout = 0;
  return (base::StringToInt(value.c_str(), &swap_timeout) ? swap_timeout : 0);
}

// Returns the minimum number of renderer frames needed before swap is
// triggered. The default value returned is 2 and is configurable by finch
// params. The reason for default being 2 is, the background image requests are
// always issued only after the first non-empty layout(first recalc style) and
// hence most of the page loads doesn't have background images rendered in their
// first frame.
int BackgroundContentViewHelper::GetNumFramesNeededForSwap(JNIEnv* env,
                                                           jobject jobj) {
  std::string value = variations::GetVariationParamValue(
      "InstantSearchClicks", "num_frames_needed_for_swap");
  int num_frames_needed = 0;
  return (base::StringToInt(value.c_str(),
                            &num_frames_needed) ? num_frames_needed : 2);
}

static jlong Init(JNIEnv* env, jobject obj) {
  return reinterpret_cast<long>(new BackgroundContentViewHelper());
}

bool BackgroundContentViewHelper::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
