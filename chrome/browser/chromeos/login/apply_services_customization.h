// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_APPLY_SERVICES_CUSTOMIZATION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_APPLY_SERVICES_CUSTOMIZATION_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

class FilePath;
class PrefService;

namespace chromeos {

// This class fetches services customization document and apply it
// as soon as the document is downloaded.
class ApplyServicesCustomization : public URLFetcher::Delegate {
 public:
  // This method checks if service customization has been applied and if not
  // starts the process.
  static void StartIfNeeded();

  // Registers preferences.
  static void RegisterPrefs(PrefService* local_state);

  // Returns true if service customization has been applied.
  static bool IsApplied();

 private:
  explicit ApplyServicesCustomization(const std::string& url_str);

  // Initiate URL fetch, return true if the object will delete itself later.
  bool Init();

  // Initiate file fetching.
  void StartFileFetch();

  // Overriden from URLFetcher::Delegate:
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // Applies given |manifest|.
  void Apply(const std::string& manifest);

  // Applies given |manifest| and delete this object.
  void ApplyAndDelete(const std::string& manifest);

  // Executes on FILE thread and reads file to string.
  void ReadFileInBackground(const FilePath& file);

  // Remember in local state status of kServicesCustomizationAppliedPref.
  static void SetApplied(bool val);

  // Services customization manifest URL.
  GURL url_;

  // URLFetcher instance.
  scoped_ptr<URLFetcher> url_fetcher_;

  // Timer to retry fetching file if network is not available.
  base::OneShotTimer<ApplyServicesCustomization> retry_timer_;

  // How many times we already tried to fetch customization manifest file.
  int num_retries_;

  DISALLOW_COPY_AND_ASSIGN(ApplyServicesCustomization);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_APPLY_SERVICES_CUSTOMIZATION_H_
