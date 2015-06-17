// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enhanced_bookmarks/bookmark_image_service_ios.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_model.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_utils.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/referrer_util.h"
#include "ios/web/public/web_state/js/crw_js_injection_evaluator.h"
#include "ios/web/public/web_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/device_form_factor.h"
#include "url/gurl.h"

namespace {

size_t kMaxNumberCachedImageTablet = 25;
size_t kMaxNumberCachedImageHandset = 12;

scoped_refptr<enhanced_bookmarks::ImageRecord> ResizeImageInternalTask(
    CGSize size,
    bool darkened,
    scoped_refptr<enhanced_bookmarks::ImageRecord> image_record) {
  UIImage* result = image_record->image->ToUIImage();

  if (!CGSizeEqualToSize(size, CGSizeZero) &&
      !CGSizeEqualToSize(size, result.size)) {
    result = ResizeImage(result, size, ProjectionMode::kAspectFill);
  }

  if (darkened)
    result = DarkenImage(result);

  if (result != image_record->image->ToUIImage())
    image_record->image.reset(new gfx::Image([result retain]));

  return image_record;
}

void ResizeImageInternal(
    base::TaskRunner* task_runner,
    CGSize size,
    bool darkened,
    enhanced_bookmarks::BookmarkImageService::ImageCallback callback,
    scoped_refptr<enhanced_bookmarks::ImageRecord> image_record) {
  const gfx::Size gfx_size(size);

  if (image_record->image->IsEmpty()) {
    callback.Run(image_record);
  } else if ((CGSizeEqualToSize(size, CGSizeZero) ||
              gfx_size == image_record->image->Size()) &&
             !darkened) {
    callback.Run(image_record);
  } else {
    base::Callback<scoped_refptr<enhanced_bookmarks::ImageRecord>(void)> task =
        base::Bind(&ResizeImageInternalTask, size, darkened, image_record);

    base::PostTaskAndReplyWithResult(task_runner, FROM_HERE, task, callback);
  }
}

// Returns a salient image URL and a referrer for the JSON string provided.
std::pair<GURL, web::Referrer> RetrieveSalientImageFromJSON(
    const std::string& json,
    const GURL& page_url,
    std::set<GURL>* in_progress_page_urls) {
  DCHECK(in_progress_page_urls);
  std::pair<GURL, web::Referrer> empty_result =
      std::make_pair(GURL(), web::Referrer());
  if (!json.length())
    return empty_result;

  scoped_ptr<base::Value> jsonData;
  int errorCode = 0;
  std::string errorMessage;
  jsonData = base::JSONReader::ReadAndReturnError(json, base::JSON_PARSE_RFC,
                                                  &errorCode, &errorMessage);
  if (errorCode || !jsonData) {
    LOG(WARNING) << "JSON parse error: " << errorMessage.c_str() << json;
    return empty_result;
  }

  base::DictionaryValue* dict;
  if (!jsonData->GetAsDictionary(&dict)) {
    LOG(WARNING) << "JSON parse error, not a dict: " << json;
    return empty_result;
  }
  std::string referrerPolicy;
  std::string image_url;
  dict->GetString("referrerPolicy", &referrerPolicy);
  dict->GetString("imageUrl", &image_url);

  // The value is lower-cased on the JS side so comparison can be exact.
  // Any unknown value is treated as default.
  web::ReferrerPolicy policy = web::ReferrerPolicyDefault;
  if (referrerPolicy == "never")
    policy = web::ReferrerPolicyNever;
  if (referrerPolicy == "always")
    policy = web::ReferrerPolicyAlways;
  if (referrerPolicy == "origin")
    policy = web::ReferrerPolicyOrigin;

  in_progress_page_urls->insert(page_url);

  web::Referrer referrer(page_url, policy);
  return std::make_pair(GURL(image_url), referrer);
}
}  // namespace

class BookmarkImageServiceIOS::MRUKey {
 public:
  MRUKey(const GURL& page_url, const CGSize& size, bool darkened)
      : page_url_(page_url), size_(size), darkened_(darkened) {}
  ~MRUKey() {}

  bool operator<(const MRUKey& rhs) const {
    if (page_url_ != rhs.page_url_)
      return page_url_ < rhs.page_url_;
    if (size_.height != rhs.size_.height)
      return size_.height < rhs.size_.height;
    if (size_.width != rhs.size_.width)
      return size_.width < rhs.size_.width;
    return darkened_ < rhs.darkened_;
  }

 private:
  const GURL page_url_;
  const CGSize size_;
  const bool darkened_;
};

BookmarkImageServiceIOS::BookmarkImageServiceIOS(
    const base::FilePath& path,
    enhanced_bookmarks::EnhancedBookmarkModel* enhanced_bookmark_model,
    net::URLRequestContextGetter* context,
    scoped_refptr<base::SequencedWorkerPool> store_pool)
    : BookmarkImageService(path, enhanced_bookmark_model, store_pool),
      imageFetcher_(new image_fetcher::ImageFetcher(store_pool)),
      pool_(store_pool),
      weak_ptr_factory_(this) {
  imageFetcher_->SetRequestContextGetter(context);
  cache_.reset(new base::MRUCache<MRUKey, MRUValue>(
      IsIPadIdiom() ? kMaxNumberCachedImageTablet
                    : kMaxNumberCachedImageHandset));
}

BookmarkImageServiceIOS::BookmarkImageServiceIOS(
    scoped_ptr<ImageStore> store,
    enhanced_bookmarks::EnhancedBookmarkModel* enhanced_bookmark_model,
    net::URLRequestContextGetter* context,
    scoped_refptr<base::SequencedWorkerPool> store_pool)
    : BookmarkImageService(store.Pass(), enhanced_bookmark_model, store_pool),
      imageFetcher_(new image_fetcher::ImageFetcher(store_pool)),
      pool_(store_pool),
      weak_ptr_factory_(this) {
  imageFetcher_->SetRequestContextGetter(context);
  cache_.reset(new base::MRUCache<MRUKey, MRUValue>(
      IsIPadIdiom() ? kMaxNumberCachedImageTablet
                    : kMaxNumberCachedImageHandset));
}

BookmarkImageServiceIOS::~BookmarkImageServiceIOS() {
}

scoped_ptr<gfx::Image> BookmarkImageServiceIOS::ResizeImage(
    const gfx::Image& image) {
  // Figure out the maximum dimension of images used on this device. On handset
  // this is the width of the view, depending on rotation. So it is the longest
  // screen size.
  UIScreen* mainScreen = [UIScreen mainScreen];
  DCHECK(mainScreen);

  // Capture the max_width in pixels.
  CGFloat max_width =
      std::max(mainScreen.bounds.size.height, mainScreen.bounds.size.width) *
      mainScreen.scale;
  DCHECK(max_width > 0);
  // On tablet the view is half the width of the screen.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
    max_width = max_width / 2.0;

  UIImage* ui_image = image.ToUIImage();

  // Images already fitting the desired size are left untouched.
  if (image.Width() < max_width || image.Height() < max_width) {
    // This enforce the creation of a new image with a new representation
    // instead of having the image share an internal representation. This is to
    // avoid a crash when the internal representation of a gfx::Image is
    // refcounted on multiple threads.
    return scoped_ptr<gfx::Image>(new gfx::Image([ui_image retain]));
  }

  // Adjust the max_width to be in the same reference model as the
  // UIImage.
  max_width = max_width / ui_image.scale;

  CGSize desired_size = CGSizeMake(max_width, max_width);

  return scoped_ptr<gfx::Image>(new gfx::Image([ ::ResizeImage(
      ui_image, desired_size, ProjectionMode::kAspectFillNoClipping) retain]));
}

// IOS does WebP transcoding as none of the platform frameworks supports this
// format natively. For this reason RetrieveSalientImageForPageUrl() needs to
// use ImageFetcher which is doing the transcoding during download.
void BookmarkImageServiceIOS::RetrieveSalientImage(
    const GURL& page_url,
    const GURL& image_url,
    const std::string& referrer,
    net::URLRequest::ReferrerPolicy referrer_policy,
    bool update_bookmark) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsPageUrlInProgress(page_url));

  const bookmarks::BookmarkNode* bookmark =
      enhanced_bookmark_model_->bookmark_model()
          ->GetMostRecentlyAddedUserNodeForURL(page_url);
  if (!bookmark) {
    ProcessNewImage(page_url, update_bookmark, image_url,
                    scoped_ptr<gfx::Image>(new gfx::Image()));
    return;
  }

  const GURL page_url_not_ref(page_url);

  // From the URL request the image asynchronously.
  image_fetcher::ImageFetchedCallback callback =
      ^(const GURL& local_image_url, int response_code, NSData* data) {
        DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
        // Build the UIImage.
        UIImage* image = nil;
        if (data) {
          image =
              [UIImage imageWithData:data scale:[UIScreen mainScreen].scale];
        }

        ProcessNewImage(page_url_not_ref, update_bookmark, local_image_url,
                        scoped_ptr<gfx::Image>(new gfx::Image([image retain])));
      };

  if (image_url.is_valid())
    imageFetcher_->StartDownload(image_url, callback, referrer,
                                 referrer_policy);
  else
    ProcessNewImage(page_url, update_bookmark, image_url,
                    scoped_ptr<gfx::Image>(new gfx::Image()));
}

void BookmarkImageServiceIOS::RetrieveSalientImageFromContext(
    id<CRWJSInjectionEvaluator> page_context,
    const GURL& page_url,
    bool update_bookmark) {
  DCHECK(CalledOnValidThread());
  if (IsPageUrlInProgress(page_url))
    return;  // A request for this URL is already in progress.

  const bookmarks::BookmarkNode* bookmark =
      enhanced_bookmark_model_->bookmark_model()
          ->GetMostRecentlyAddedUserNodeForURL(page_url);
  if (!bookmark)
    return;

  if (!experimental_flags::IsBookmarkImageFetchingOnVisitEnabled()) {
    // Stop the image extraction if there is already an image present.
    GURL url;
    int height, width;

    // This test below is not ideal : Having an URL for an image is not quite
    // the same thing as having successfuly downloaded that URL. Testing for the
    // presence of the downloaded image itself would be quite costly as it would
    // require jumping to another thread to access the image store. Also if the
    // user has bookmarks, but never opened the bookmark UI, there will be no
    // image yet, so even that test would be incomplete.
    if (enhanced_bookmark_model_->GetOriginalImage(bookmark, &url, &width,
                                                   &height) ||
        enhanced_bookmark_model_->GetThumbnailImage(bookmark, &url, &width,
                                                    &height))
      return;
  }

  if (!script_) {
    NSString* path = [base::mac::FrameworkBundle()
        pathForResource:@"bookmark_image_service_ios"
                 ofType:@"js"];
    DCHECK(path) << "bookmark_image_service_ios script file not found.";
    script_.reset([[NSString stringWithContentsOfFile:path
                                             encoding:NSUTF8StringEncoding
                                                error:nil] copy]);
  }

  if (!script_) {
    LOG(WARNING) << "Unable to load bookmark_images_ios resource";
    return;
  }

  // Since |page_url| is a reference make a copy since it will be used inside a
  // block.
  const GURL page_url_copy = page_url;
  // Since a method on |this| is called from a block, this dance is necessary to
  // make sure a method on |this| is not called when the object has gone away.
  base::WeakPtr<BookmarkImageServiceIOS> weak_this =
      weak_ptr_factory_.GetWeakPtr();

  [page_context evaluateJavaScript:script_
               stringResultHandler:^(NSString* result, NSError* error) {
                 if (!weak_this)
                   return;
                 // The script returns a json dict with just an image URL and
                 // the referrer policy for the page.
                 std::string json = base::SysNSStringToUTF8(result);
                 std::pair<GURL, web::Referrer> image_load_info =
                     RetrieveSalientImageFromJSON(json, page_url_copy,
                                                  &in_progress_page_urls_);
                 if (image_load_info.first.is_empty())
                   return;
                 RetrieveSalientImage(
                     page_url_copy, image_load_info.first,
                     web::ReferrerHeaderValueForNavigation(
                         image_load_info.first, image_load_info.second),
                     web::PolicyForNavigation(image_load_info.first,
                                              image_load_info.second),
                     update_bookmark);
               }];
}

void BookmarkImageServiceIOS::FinishSuccessfulPageLoadForNativationItem(
    id<CRWJSInjectionEvaluator> page_context,
    web::NavigationItem* navigation_item,
    const GURL& original_url) {
  DCHECK(CalledOnValidThread());
  DCHECK(navigation_item);

  // If the navigation is a simple back or forward, do not extract images, those
  // were extracted already.
  if (navigation_item->GetTransitionType() & ui::PAGE_TRANSITION_FORWARD_BACK)
    return;

  std::vector<GURL> urls;
  urls.push_back(navigation_item->GetURL());
  if (navigation_item->GetURL() != original_url)
    urls.push_back(original_url);

  for (auto url : urls) {
    if (enhanced_bookmark_model_->bookmark_model()->IsBookmarked(url)) {
      RetrieveSalientImageFromContext(page_context, url, true);
    }
  }
}

void BookmarkImageServiceIOS::ReturnAndCache(
    GURL page_url,
    CGSize size,
    bool darkened,
    ImageCallback callback,
    scoped_refptr<enhanced_bookmarks::ImageRecord> image_record) {
  cache_->Put(MRUKey(page_url, size, darkened), image_record);

  callback.Run(image_record);
}

void BookmarkImageServiceIOS::SalientImageResizedForUrl(
    const GURL& page_url,
    const CGSize size,
    bool darkened,
    const ImageCallback& callback) {
  MRUKey tuple = MRUKey(page_url, size, darkened);
  base::MRUCache<MRUKey, MRUValue>::iterator it = cache_->Get(tuple);
  if (it != cache_->end()) {
    callback.Run(it->second);
  } else {
    SalientImageForUrl(
        page_url,
        base::Bind(&ResizeImageInternal, pool_, size, darkened,
                   base::Bind(&BookmarkImageServiceIOS::ReturnAndCache,
                              base::Unretained(this), page_url, size, darkened,
                              callback)));
  }
}
