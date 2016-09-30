// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bookmarks/partner_bookmarks_reader.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/browser_thread.h"
#include "jni/PartnerBookmarksReader_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;
using bookmarks::BookmarkNode;
using bookmarks::BookmarkPermanentNode;
using content::BrowserThread;

namespace {

void SetFaviconTask(Profile* profile,
                    const GURL& page_url, const GURL& icon_url,
                    const std::vector<unsigned char>& image_data,
                    favicon_base::IconType icon_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<base::RefCountedMemory> bitmap_data(
      new base::RefCountedBytes(image_data));
  gfx::Size pixel_size(gfx::kFaviconSize, gfx::kFaviconSize);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile(),
          ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;

  favicon_service->MergeFavicon(
      page_url, page_url, icon_type, bitmap_data, pixel_size);
}

void SetFaviconCallback(Profile* profile,
                        const GURL& page_url, const GURL& icon_url,
                        const std::vector<unsigned char>& image_data,
                        favicon_base::IconType icon_type,
                        base::WaitableEvent* bookmark_added_event) {
  SetFaviconTask(profile, page_url, icon_url, image_data, icon_type);
  if (bookmark_added_event)
    bookmark_added_event->Signal();
}

void PrepareAndSetFavicon(JNIEnv* env, jbyte* icon_bytes, int icon_len,
                          BookmarkNode* node, Profile* profile,
                          favicon_base::IconType icon_type) {
  SkBitmap icon_bitmap;
  if (!gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(icon_bytes),
      icon_len, &icon_bitmap))
    return;
  std::vector<unsigned char> image_data;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(icon_bitmap, false, &image_data))
    return;
  // TODO(aruslan): TODO(tedchoc): Follow up on how to avoid this through js.
  // Since the favicon URL is used as a key in the history's thumbnail DB,
  // this gives us a value which does not collide with others.
  GURL fake_icon_url = node->url();
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    SetFaviconTask(profile,
                   node->url(), fake_icon_url,
                   image_data, icon_type);
  } else {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SetFaviconCallback,
                   profile, node->url(), fake_icon_url,
                   image_data, icon_type, &event));
    // TODO(aruslan): http://b/6397072 If possible - avoid using favicon service
    event.Wait();
  }
}

const BookmarkNode* GetNodeByID(const BookmarkNode* parent, int64_t id) {
  if (parent->id() == id)
    return parent;
  for (int i= 0, child_count = parent->child_count(); i < child_count; ++i) {
    const BookmarkNode* result = GetNodeByID(parent->GetChild(i), id);
    if (result)
      return result;
  }
  return NULL;
}

}  // namespace

PartnerBookmarksReader::PartnerBookmarksReader(
    PartnerBookmarksShim* partner_bookmarks_shim,
    Profile* profile)
    : partner_bookmarks_shim_(partner_bookmarks_shim),
      profile_(profile),
      wip_next_available_id_(0) {
}

PartnerBookmarksReader::~PartnerBookmarksReader() {}

void PartnerBookmarksReader::PartnerBookmarksCreationComplete(
    JNIEnv*,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  partner_bookmarks_shim_->SetPartnerBookmarksRoot(
      std::move(wip_partner_bookmarks_root_));
  wip_next_available_id_ = 0;
}

void PartnerBookmarksReader::Destroy(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  delete this;
}

void PartnerBookmarksReader::Reset(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  wip_partner_bookmarks_root_.reset();
  wip_next_available_id_ = 0;
}

jlong PartnerBookmarksReader::AddPartnerBookmark(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jurl,
    const JavaParamRef<jstring>& jtitle,
    jboolean is_folder,
    jlong parent_id,
    const JavaParamRef<jbyteArray>& favicon,
    const JavaParamRef<jbyteArray>& touchicon) {
  base::string16 url;
  base::string16 title;
  if (jurl)
    url = ConvertJavaStringToUTF16(env, jurl);
  if (jtitle)
    title = ConvertJavaStringToUTF16(env, jtitle);

  jlong node_id = 0;
  if (wip_partner_bookmarks_root_.get()) {
    std::unique_ptr<BookmarkNode> node =
        base::MakeUnique<BookmarkNode>(wip_next_available_id_++, GURL(url));
    node->set_type(is_folder ? BookmarkNode::FOLDER : BookmarkNode::URL);
    node->SetTitle(title);

    // Handle favicon and touchicon
    if (profile_ != nullptr && (favicon != nullptr || touchicon != nullptr)) {
      jbyteArray icon = (touchicon != nullptr) ? touchicon : favicon;
      const favicon_base::IconType icon_type =
          touchicon ? favicon_base::TOUCH_ICON : favicon_base::FAVICON;
      const int icon_len = env->GetArrayLength(icon);
      jbyte* icon_bytes = env->GetByteArrayElements(icon, nullptr);
      if (icon_bytes)
        PrepareAndSetFavicon(env, icon_bytes, icon_len, node.get(), profile_,
                             icon_type);
      env->ReleaseByteArrayElements(icon, icon_bytes, JNI_ABORT);
    }

    const BookmarkNode* parent =
        GetNodeByID(wip_partner_bookmarks_root_.get(), parent_id);
    if (!parent) {
      LOG(WARNING) << "partner_bookmarks_shim: invalid/unknown parent_id="
                   << parent_id << ": adding to the root";
      parent = wip_partner_bookmarks_root_.get();
    }
    node_id = node->id();
    const_cast<BookmarkNode*>(parent)->Add(std::move(node),
                                           parent->child_count());
  } else {
    std::unique_ptr<BookmarkPermanentNode> node =
        base::MakeUnique<BookmarkPermanentNode>(wip_next_available_id_++);
    node_id = node->id();
    node->SetTitle(title);
    wip_partner_bookmarks_root_ = std::move(node);
  }
  return node_id;
}

// static
static void DisablePartnerBookmarksEditing(JNIEnv* env,
                                           const JavaParamRef<jclass>& clazz) {
  PartnerBookmarksShim::DisablePartnerBookmarksEditing();
}

// static
bool PartnerBookmarksReader::RegisterPartnerBookmarksReader(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// ----------------------------------------------------------------

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  PartnerBookmarksShim* partner_bookmarks_shim =
      PartnerBookmarksShim::BuildForBrowserContext(profile);
  PartnerBookmarksReader* reader = new PartnerBookmarksReader(
      partner_bookmarks_shim, profile);
  return reinterpret_cast<intptr_t>(reader);
}
