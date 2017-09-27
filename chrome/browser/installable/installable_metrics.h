// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLABLE_METRICS_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLABLE_METRICS_H_

#include <memory>

// This enum backs a UMA histogram and must be treated as append-only.
enum class InstallabilityCheckStatus {
  NOT_STARTED,
  IN_PROGRESS_UNKNOWN,
  IN_PROGRESS_NON_PROGRESSIVE_WEB_APP,
  IN_PROGRESS_PROGRESSIVE_WEB_APP,
  COMPLETE_NON_PROGRESSIVE_WEB_APP,
  COMPLETE_PROGRESSIVE_WEB_APP,
  COUNT,
};

// This enum backs a UMA histogram and must be treated as append-only.
enum class AddToHomescreenTimeoutStatus {
  NO_TIMEOUT_NON_PROGRESSIVE_WEB_APP,
  NO_TIMEOUT_PROGRESSIVE_WEB_APP,
  TIMEOUT_MANIFEST_FETCH_NON_PROGRESSIVE_WEB_APP,
  TIMEOUT_MANIFEST_FETCH_PROGRESSIVE_WEB_APP,
  TIMEOUT_MANIFEST_FETCH_UNKNOWN,
  TIMEOUT_INSTALLABILITY_CHECK_NON_PROGRESSIVE_WEB_APP,
  TIMEOUT_INSTALLABILITY_CHECK_PROGRESSIVE_WEB_APP,
  TIMEOUT_INSTALLABILITY_CHECK_UNKNOWN,
  COUNT,
};

class InstallableMetrics {
 public:
  class Recorder {
   public:
    virtual ~Recorder() {}
    virtual void Resolve(bool check_passed) {}
    virtual void Flush(bool waiting_for_service_worker) {}

    virtual void RecordMenuOpen() = 0;
    virtual void RecordMenuItemAddToHomescreen() = 0;
    virtual void RecordAddToHomescreenNoTimeout() = 0;
    virtual void RecordAddToHomescreenManifestAndIconTimeout() = 0;
    virtual void RecordAddToHomescreenInstallabilityTimeout() = 0;
    virtual void Start() = 0;
  };

  InstallableMetrics();
  ~InstallableMetrics();

  // This records the state of the installability check when the Android menu is
  // opened.
  void RecordMenuOpen();

  // This records the state of the installability check when the add to
  // homescreen menu item on Android is tapped.
  void RecordMenuItemAddToHomescreen();

  // Called to record the add to homescreen dialog not timing out on the
  // InstallableManager work.
  void RecordAddToHomescreenNoTimeout();

  // Called to record the add to homescreen dialog timing out during the
  // manifest and icon fetch.
  void RecordAddToHomescreenManifestAndIconTimeout();

  // Called to record the add to homescreen dialog timing out on the service
  // worker + installability check.
  void RecordAddToHomescreenInstallabilityTimeout();

  // Called to resolve any queued metrics from incomplete tasks.
  void Resolve(bool check_passed);

  // Called to save any queued metrics.
  void Flush(bool waiting_for_service_worker);

  // Called to indicate that the InstallableManager has started working on the
  // current page.
  void Start();

 private:
  std::unique_ptr<Recorder> recorder_;
};

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_METRICS_H_
