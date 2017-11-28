// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_task_impl.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "base/mac/bind_objc_block.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/net/cookies/wk_cookie_util.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/error_translation_util.h"
#include "net/base/filename_util.h"
#include "net/base/io_buffer.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher_response_writer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// CRWURLSessionDelegate block types.
using PropertiesBlock = void (^)(NSURLSessionTask*, NSError*);
using DataBlock = void (^)(scoped_refptr<net::IOBufferWithSize> buffer);

// Translates an CFNetwork error code to a net error code. Returns 0 if |error|
// is nil.
int GetNetErrorCodeFromNSError(NSError* error) {
  int error_code = 0;
  if (error) {
    if (!web::GetNetErrorFromIOSErrorCode(error.code, &error_code)) {
      error_code = net::ERR_FAILED;
    }
  }
  return error_code;
}

// Creates a new buffer from raw |data| and |size|.
scoped_refptr<net::IOBufferWithSize> GetBuffer(const void* data, size_t size) {
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(size);
  memcpy(buffer->data(), data, size);
  return buffer;
}

// Percent complete for the given NSURLSessionTask within [0..100] range.
int GetTaskPercentComplete(NSURLSessionTask* task) {
  DCHECK(task);
  if (!task.countOfBytesExpectedToReceive) {
    return 100;
  }
  DCHECK_GE(task.countOfBytesExpectedToReceive, task.countOfBytesReceived);
  return 100.0 * task.countOfBytesReceived / task.countOfBytesExpectedToReceive;
}

// Used as no-op callback.
void DoNothing(int) {}

}  // namespace

// NSURLSessionDataDelegate that forwards data and properties task updates to
// the client. Client of this delegate can pass blocks to receive the updates.
@interface CRWURLSessionDelegate : NSObject<NSURLSessionDataDelegate>
- (instancetype)init NS_UNAVAILABLE;
// Initializes delegate with blocks. |propertiesBlock| is called when
// DownloadTaskImpl should update its properties (is_done, error_code,
// total_bytes, and percent_complete) and call OnDownloadUpdated callback.
// |dataBlock| is called when DownloadTaskImpl should write a chunk of
// downloaded data.
- (instancetype)initWithPropertiesBlock:(PropertiesBlock)propertiesBlock
                              dataBlock:(DataBlock)dataBlock
    NS_DESIGNATED_INITIALIZER;
@end

@implementation CRWURLSessionDelegate {
  PropertiesBlock _propertiesBlock;
  DataBlock _dataBlock;
}

- (instancetype)initWithPropertiesBlock:(PropertiesBlock)propertiesBlock
                              dataBlock:(DataBlock)dataBlock {
  DCHECK(propertiesBlock);
  DCHECK(dataBlock);
  if ((self = [super init])) {
    _propertiesBlock = propertiesBlock;
    _dataBlock = dataBlock;
  }
  return self;
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(nullable NSError*)error {
  _propertiesBlock(task, error);
}

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)task
    didReceiveData:(NSData*)data {
  using Bytes = const void* _Nonnull;
  [data enumerateByteRangesUsingBlock:^(Bytes bytes, NSRange range, BOOL*) {
    auto buffer = GetBuffer(bytes, range.length);
    _dataBlock(std::move(buffer));
  }];
  _propertiesBlock(task, nil);
}

- (void)URLSession:(NSURLSession*)session
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential* _Nullable))handler {
  // TODO(crbug.com/780911): use CRWCertVerificationController to get
  // CertAcceptPolicy for this |challenge|.
  handler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
}

@end

namespace web {

DownloadTaskImpl::DownloadTaskImpl(const WebState* web_state,
                                   const GURL& original_url,
                                   const std::string& content_disposition,
                                   int64_t total_bytes,
                                   const std::string& mime_type,
                                   NSString* identifier,
                                   Delegate* delegate)
    : original_url_(original_url),
      total_bytes_(total_bytes),
      content_disposition_(content_disposition),
      mime_type_(mime_type),
      web_state_(web_state),
      delegate_(delegate),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  session_ = CreateSession(identifier);
  DCHECK(web_state_);
  DCHECK(delegate_);
  DCHECK(session_);
}

DownloadTaskImpl::~DownloadTaskImpl() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (delegate_) {
    delegate_->OnTaskDestroyed(this);
  }
  ShutDown();
}

void DownloadTaskImpl::ShutDown() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [session_task_ cancel];
  session_task_ = nil;
  delegate_ = nullptr;
}

void DownloadTaskImpl::Start(
    std::unique_ptr<net::URLFetcherResponseWriter> writer) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(!writer_);
  writer_ = std::move(writer);
  GetCookies(base::Bind(&DownloadTaskImpl::StartWithCookies,
                        weak_factory_.GetWeakPtr()));
}

net::URLFetcherResponseWriter* DownloadTaskImpl::GetResponseWriter() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return writer_.get();
}

NSString* DownloadTaskImpl::GetIndentifier() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return session_.configuration.identifier;
}

const GURL& DownloadTaskImpl::GetOriginalUrl() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return original_url_;
}

bool DownloadTaskImpl::IsDone() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return is_done_;
}

int DownloadTaskImpl::GetErrorCode() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return error_code_;
}

int64_t DownloadTaskImpl::GetTotalBytes() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return total_bytes_;
}

int DownloadTaskImpl::GetPercentComplete() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return percent_complete_;
}

std::string DownloadTaskImpl::GetContentDisposition() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return content_disposition_;
}

std::string DownloadTaskImpl::GetMimeType() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return mime_type_;
}

base::string16 DownloadTaskImpl::GetSuggestedFilename() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return net::GetSuggestedFilename(GetOriginalUrl(), GetContentDisposition(),
                                   /*referrer_charset=*/std::string(),
                                   /*suggested_name=*/std::string(),
                                   /*mime_type=*/std::string(),
                                   /*default_name=*/"document");
}

void DownloadTaskImpl::AddObserver(DownloadTaskObserver* observer) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void DownloadTaskImpl::RemoveObserver(DownloadTaskObserver* observer) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

NSURLSession* DownloadTaskImpl::CreateSession(NSString* identifier) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(identifier.length);
  base::WeakPtr<DownloadTaskImpl> weak_this = weak_factory_.GetWeakPtr();
  id<NSURLSessionDataDelegate> session_delegate = [[CRWURLSessionDelegate alloc]
      initWithPropertiesBlock:^(NSURLSessionTask* task, NSError* error) {
        if (!weak_this.get()) {
          return;
        }

        error_code_ = GetNetErrorCodeFromNSError(error);
        percent_complete_ = GetTaskPercentComplete(task);
        total_bytes_ = task.countOfBytesExpectedToReceive;
        if (task.response.MIMEType)
          mime_type_ = base::SysNSStringToUTF8(task.response.MIMEType);

        if (task.state != NSURLSessionTaskStateCompleted) {
          OnDownloadUpdated();
          // Download is still in progress, nothing to do here.
          return;
        }

        // Download has finished, so finalize the writer and signal completion.
        auto callback = base::Bind(&DownloadTaskImpl::OnDownloadFinished,
                                   weak_factory_.GetWeakPtr());
        if (writer_->Finish(error_code_, callback) != net::ERR_IO_PENDING) {
          OnDownloadFinished(error_code_);
        }
      }
      dataBlock:^(scoped_refptr<net::IOBufferWithSize> buffer) {
        if (weak_this.get()) {
          // Ignore Write's completion handler. |dataBlock| may be called
          // multiple times for a single downloaded chunk of data and there is
          // no need to wait for writing completion.
          writer_->Write(buffer.get(), buffer->size(), base::Bind(&DoNothing));
        }
      }];
  return delegate_->CreateSession(identifier, session_delegate,
                                  NSOperationQueue.currentQueue);
}

void DownloadTaskImpl::GetCookies(
    base::Callback<void(NSArray<NSHTTPCookie*>*)> callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (@available(iOS 11, *)) {
    GetWKCookies(callback);
  } else {
    WebThread::PostTask(WebThread::UI, FROM_HERE, base::BindBlockArc(^{
                          callback.Run([NSArray array]);
                        }));
  }
}

void DownloadTaskImpl::GetWKCookies(
    base::Callback<void(NSArray<NSHTTPCookie*>*)> callback) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  auto store = WKCookieStoreForBrowserState(web_state_->GetBrowserState());
  DCHECK(store);
  [store getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
    // getAllCookies: callback is always called on UI thread.
    DCHECK_CURRENTLY_ON(WebThread::UI);
    callback.Run(cookies);
  }];
}

void DownloadTaskImpl::StartWithCookies(NSArray<NSHTTPCookie*>* cookies) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(writer_);

  NSURL* url = net::NSURLWithGURL(GetOriginalUrl());
  session_task_ = [session_ dataTaskWithURL:url];
  [session_.configuration.HTTPCookieStorage storeCookies:cookies
                                                 forTask:session_task_];
  [session_task_ resume];
  OnDownloadUpdated();
}

void DownloadTaskImpl::OnDownloadUpdated() {
  for (auto& observer : observers_)
    observer.OnDownloadUpdated(this);
}

void DownloadTaskImpl::OnDownloadFinished(int error_code) {
  is_done_ = true;
  session_task_ = nil;
  OnDownloadUpdated();
}

}  // namespace web
