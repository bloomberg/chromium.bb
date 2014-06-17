// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/profiles/profile_downloader_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "jni/ProfileDownloader_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"

namespace {

// An account fetcher callback.
class AccountInfoRetriever : public ProfileDownloaderDelegate {
 public:
  explicit AccountInfoRetriever(Profile* profile,
                                const std::string& account_id,
                                const int desired_image_side_pixels)
      : profile_(profile),
        account_id_(account_id),
        desired_image_side_pixels_(desired_image_side_pixels) {}

  void Start() {
    profile_image_downloader_.reset(new ProfileDownloader(this));
    profile_image_downloader_->StartForAccount(account_id_);
  }

  void Shutdown() {
    profile_image_downloader_.reset();
    delete this;
  }

  // ProfileDownloaderDelegate implementation:
  virtual bool NeedsProfilePicture() const OVERRIDE {
    return desired_image_side_pixels_ > 0;
  }

  virtual int GetDesiredImageSideLength() const OVERRIDE {
    return desired_image_side_pixels_;
  }

  virtual Profile* GetBrowserProfile() OVERRIDE {
    return profile_;
  }

  virtual std::string GetCachedPictureURL() const OVERRIDE {
    return std::string();
  }

  virtual void OnProfileDownloadSuccess(
      ProfileDownloader* downloader) OVERRIDE {
    ProfileDownloaderAndroid::OnProfileDownloadSuccess(
        account_id_,
        downloader->GetProfileFullName(),
        downloader->GetProfilePicture());
    Shutdown();
  }

  virtual void OnProfileDownloadFailure(
      ProfileDownloader* downloader,
      ProfileDownloaderDelegate::FailureReason reason) OVERRIDE {
    LOG(ERROR) << "Failed to download the profile information: " << reason;
    Shutdown();
  }

  // The profile image downloader instance.
  scoped_ptr<ProfileDownloader> profile_image_downloader_;

  // The browser profile associated with this download request.
  Profile* profile_;

  // The account ID (email address) to be loaded.
  const std::string account_id_;

  // Desired side length of the profile image (in pixels).
  const int desired_image_side_pixels_;

  DISALLOW_COPY_AND_ASSIGN(AccountInfoRetriever);
};

}  // namespace

// static
void ProfileDownloaderAndroid::OnProfileDownloadSuccess(
    const std::string& account_id,
    const base::string16& full_name,
    const SkBitmap& bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jbitmap;
  if (!bitmap.isNull() && bitmap.bytesPerPixel() != 0)
    jbitmap = gfx::ConvertToJavaBitmap(&bitmap);
  Java_ProfileDownloader_onProfileDownloadSuccess(
      env,
      base::android::ConvertUTF8ToJavaString(env, account_id).obj(),
      base::android::ConvertUTF16ToJavaString(env, full_name).obj(),
      jbitmap.obj());
}

// static
jstring GetCachedNameForPrimaryAccount(JNIEnv* env,
                                       jclass clazz,
                                       jobject jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  ProfileInfoInterface& info =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  const size_t index = info.GetIndexOfProfileWithPath(profile->GetPath());

  base::string16 name;
  if (index != std::string::npos)
    name = info.GetGAIANameOfProfileAtIndex(index);

  return base::android::ConvertUTF16ToJavaString(env, name).Release();
}

// static
jobject GetCachedAvatarForPrimaryAccount(JNIEnv* env,
                                         jclass clazz,
                                         jobject jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  ProfileInfoInterface& info =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  const size_t index = info.GetIndexOfProfileWithPath(profile->GetPath());

  ScopedJavaLocalRef<jobject> jbitmap;
  if (index != std::string::npos) {
    gfx::Image avatar_image = info.GetAvatarIconOfProfileAtIndex(index);
    if (!avatar_image.IsEmpty() &&
        avatar_image.Width() > profiles::kAvatarIconWidth &&
        avatar_image.Height() > profiles::kAvatarIconHeight &&
        avatar_image.AsImageSkia().bitmap()) {
      jbitmap = gfx::ConvertToJavaBitmap(avatar_image.AsImageSkia().bitmap());
    }
  }

  return jbitmap.Release();
}

// static
void StartFetchingAccountInfoFor(
    JNIEnv* env,
    jclass clazz,
    jobject jprofile,
    jstring jaccount_id,
    jint image_side_pixels) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  const std::string account_id =
      base::android::ConvertJavaStringToUTF8(env, jaccount_id);
  AccountInfoRetriever* retriever =
      new AccountInfoRetriever(profile, account_id, image_side_pixels);
  retriever->Start();
}

// static
bool ProfileDownloaderAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
