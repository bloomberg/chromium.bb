// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWER_TRANSLATE_DOWNLOAD_MANAGER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWER_TRANSLATE_DOWNLOAD_MANAGER_H_

#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "net/url_request/url_request_context_getter.h"

template <typename T> struct DefaultSingletonTraits;

// Manages the downloaded resources for Translate, such as the translate script
// and the language list.
// TODO(droger): TranslateDownloadManager should own TranslateLanguageList and
// TranslateScript. See http://crbug.com/335074 and http://crbug.com/335077.
class TranslateDownloadManager {
 public:
  // Returns the singleton instance.
  static TranslateDownloadManager* GetInstance();

  // The request context used to download the resources.
  // Should be set before this class can be used.
  net::URLRequestContextGetter* request_context() { return request_context_; }
  void set_request_context(net::URLRequestContextGetter* context) {
      request_context_ = context;
  }

  // The application locale.
  // Should be set before this class can be used.
  const std::string& application_locale() {
    DCHECK(!application_locale_.empty());
    return application_locale_;
  }
  void set_application_locale(const std::string& locale) {
    application_locale_ = locale;
  }

 private:
  friend struct DefaultSingletonTraits<TranslateDownloadManager>;
  TranslateDownloadManager();
  virtual ~TranslateDownloadManager();

  std::string application_locale_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
};

#endif  // COMPONENTS_TRANSLATE_CORE_BROWER_TRANSLATE_DOWNLOAD_MANAGER_H_
