// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/macros.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/power_save_blocker_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents_observer.h"
#include "jni/PowerSaveBlocker_jni.h"
#include "ui/android/view_android.h"

namespace content {

using base::android::AttachCurrentThread;

class PowerSaveBlockerImpl::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlockerImpl::Delegate>,
      public WebContentsObserver {
 public:
  Delegate(WebContents* web_contents,
           scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  // Does the actual work to apply or remove the desired power save block.
  void ApplyBlock();
  void RemoveBlock();

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  ~Delegate() override;

  base::android::ScopedJavaLocalRef<jobject> GetContentViewCore();

  base::android::ScopedJavaGlobalRef<jobject> java_power_save_blocker_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

PowerSaveBlockerImpl::Delegate::Delegate(
    WebContents* web_contents,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : WebContentsObserver(web_contents), ui_task_runner_(ui_task_runner) {
  JNIEnv* env = AttachCurrentThread();
  java_power_save_blocker_.Reset(Java_PowerSaveBlocker_create(env));
}

PowerSaveBlockerImpl::Delegate::~Delegate() {
}

void PowerSaveBlockerImpl::Delegate::ApplyBlock() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  base::android::ScopedJavaLocalRef<jobject> java_content_view_core =
      GetContentViewCore();
  if (java_content_view_core.is_null())
    return;

  ScopedJavaLocalRef<jobject> obj(java_power_save_blocker_);
  JNIEnv* env = AttachCurrentThread();
  Java_PowerSaveBlocker_applyBlock(env, obj.obj(),
                                   java_content_view_core.obj());
}

void PowerSaveBlockerImpl::Delegate::RemoveBlock() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  base::android::ScopedJavaLocalRef<jobject> java_content_view_core =
      GetContentViewCore();
  if (java_content_view_core.is_null())
    return;

  ScopedJavaLocalRef<jobject> obj(java_power_save_blocker_);
  JNIEnv* env = AttachCurrentThread();
  Java_PowerSaveBlocker_removeBlock(env, obj.obj(),
                                    java_content_view_core.obj());
}

base::android::ScopedJavaLocalRef<jobject>
PowerSaveBlockerImpl::Delegate::GetContentViewCore() {
  if (!web_contents())
    return base::android::ScopedJavaLocalRef<jobject>();

  ContentViewCoreImpl* content_view_core_impl =
      ContentViewCoreImpl::FromWebContents(web_contents());
  if (!content_view_core_impl)
    return base::android::ScopedJavaLocalRef<jobject>();

  return content_view_core_impl->GetJavaObject();
}

PowerSaveBlockerImpl::PowerSaveBlockerImpl(
    PowerSaveBlockerType type,
    Reason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : ui_task_runner_(ui_task_runner),
      blocking_task_runner_(blocking_task_runner) {
  // Don't support kPowerSaveBlockPreventAppSuspension
}

PowerSaveBlockerImpl::~PowerSaveBlockerImpl() {
  if (delegate_.get()) {
    ui_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&Delegate::RemoveBlock, delegate_));
  }
}

void PowerSaveBlockerImpl::InitDisplaySleepBlocker(WebContents* web_contents) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  if (!web_contents)
    return;

  delegate_ = new Delegate(web_contents, ui_task_runner_);
  delegate_->ApplyBlock();
}

bool RegisterPowerSaveBlocker(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
