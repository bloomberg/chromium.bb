// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_SERVICE_H_
#define COMPONENTS_DOMAIN_RELIABILITY_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "components/domain_reliability/clear_mode.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/context.h"
#include "components/domain_reliability/domain_reliability_export.h"

class GURL;

namespace base {
class Value;
}  // namespace base

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace domain_reliability {

class DomainReliabilityMonitor;

// DomainReliabilityService manages a Monitor that (could) live on another
// thread (as provided by the URLRequestContextGetter's task runner) and
// proxies (selected) method calls to it. Destruction of the Monitor (on that
// thread) is the responsibility of the caller.
class DOMAIN_RELIABILITY_EXPORT DomainReliabilityService {
 public:
  // Creates a DomainReliabilityService that will contain a Monitor with the
  // given upload reporter string.
  static DomainReliabilityService* Create(
      const std::string& upload_reporter_string);

  virtual ~DomainReliabilityService();

  // Initializes the Service: given the task runner on which Monitor methods
  // should be called, creates the Monitor and returns it. Can be called at
  // most once, and must be called before any of the below methods can be
  // called. The caller is responsible for destroying the Monitor on the given
  // task runner when it is no longer needed. |upload_allowed_callback| is
  // called on the network thread before every upload to check if it's allowed.
  virtual std::unique_ptr<DomainReliabilityMonitor> CreateMonitor(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      const DomainReliabilityContext::UploadAllowedCallback&
          upload_allowed_callback) = 0;

  // Clears browsing data on the associated Monitor. |Init()| must have been
  // called first.
  virtual void ClearBrowsingData(
      DomainReliabilityClearMode clear_mode,
      const base::Callback<bool(const GURL&)>& origin,
      const base::Closure& callback) = 0;

  virtual void GetWebUIData(
      const base::Callback<void(std::unique_ptr<base::Value>)>& callback)
      const = 0;

  virtual void SetDiscardUploadsForTesting(
      bool discard_uploads) = 0;

  virtual void AddContextForTesting(
      std::unique_ptr<const DomainReliabilityConfig> config) = 0;

  virtual void ForceUploadsForTesting() = 0;

 protected:
  DomainReliabilityService();

 private:
  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityService);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_SERVICE_H_
