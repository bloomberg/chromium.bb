// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android_safe_browsing_api_handler.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "content/public/browser/browser_thread.h"
// TODO(nparker): Include JNI-generated file

using content::BrowserThread;

AndroidSafeBrowsingAPIHandler::AndroidSafeBrowsingAPIHandler() {
  j_context_ = base::android::GetApplicationContext();
  DCHECK(j_context_);

  // TODO(nparker): Fetch API key pertaining to Chrome.
  api_key_ = "<todo-get-appropriate-api-key>";
}

bool AndroidSafeBrowsingAPIHandler::StartURLCheck(
    const URLCheckCallback& callback,
    const GURL& url,
    const std::vector<SBThreatType>& threat_types) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Make copy on the heap so we can pass the pointer through JNI.
  URLCheckCallback* callback_ptr = new URLCheckCallback(callback);
  intptr_t callback_id_int = reinterpret_cast<intptr_t>(callback_ptr);
  jlong callback_id = static_cast<jlong>(callback_id_int);

  // TODO(nparker): Reduce logging before launch of this feature.
  VLOG(1) << "Starting check " << callback_id << " for URL " << url;

  // TODO(nparker): Implement this.
  // Convert threat types to API's enums.
  // Call JNI method.

  // Until we have the Java implementation, just invoke the callback in 500ms
  // for testing.
  return BrowserThread::PostDelayedTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&OnURLCheckDone, callback_id, true, jstring()),
      base::TimeDelta::FromMilliseconds(500));
}

// static
void AndroidSafeBrowsingAPIHandler::OnURLCheckDone(jlong callback_id,
                                                   jboolean isSuccessful,
                                                   jstring metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(callback_id);

  VLOG(1) << "URLCheckDone invoked for check " << callback_id;

  // Convert java long long int to c++ pointer, take ownership.
  scoped_ptr<URLCheckCallback> callback(
      reinterpret_cast<URLCheckCallback*>(callback_id));

  // TODO(nparker):
  // 1) Convert metadata to binary proto of MalwarePatternType so
  //    SafeBrowsingUIManager::DisplayBlockingPage() can unmarshal it.
  // 2) Pick the "worst" threat type from metadata and map to SBThreatType.

  callback->Run(SB_THREAT_TYPE_SAFE, std::string());
  VLOG(1) << "Done with check " << callback_id;
}
