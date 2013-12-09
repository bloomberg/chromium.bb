// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CONNECTION_TESTER_H_
#define CHROME_BROWSER_NET_CONNECTION_TESTER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "url/gurl.h"

namespace net {
class NetLog;
class URLRequestContext;
}  // namespace net

// ConnectionTester runs a suite of tests (also called "experiments"),
// to try and discover why loading a particular URL is failing with an error
// code.
//
// For example, one reason why the URL might have failed, is that the
// network requires the URL to be routed through a proxy, however chrome is
// not configured for that.
//
// The above issue might be detected by running test that fetches the URL using
// auto-detect and seeing if it works this time. Or even by retrieving the
// settings from another installed browser and trying with those.
//
// USAGE:
//
// To run the test suite, create an instance of ConnectionTester and then call
// RunAllTests().
//
// This starts a sequence of tests, which will complete asynchronously.
// The ConnectionTester object can be deleted at any time, and it will abort
// any of the in-progress tests.
//
// As tests are started or completed, notification will be sent through the
// "Delegate" object.

class ConnectionTester {
 public:
  // This enum lists the possible proxy settings configurations.
  enum ProxySettingsExperiment {
    // Do not use any proxy.
    PROXY_EXPERIMENT_USE_DIRECT = 0,

    // Use the system proxy settings.
    PROXY_EXPERIMENT_USE_SYSTEM_SETTINGS,

    // Use Firefox's proxy settings if they are available.
    PROXY_EXPERIMENT_USE_FIREFOX_SETTINGS,

    // Use proxy auto-detect.
    PROXY_EXPERIMENT_USE_AUTO_DETECT,

    PROXY_EXPERIMENT_COUNT,
  };

  // This enum lists the possible host resolving configurations.
  enum HostResolverExperiment {
    // Use a default host resolver implementation.
    HOST_RESOLVER_EXPERIMENT_PLAIN = 0,

    // Disable IPv6 host resolving.
    HOST_RESOLVER_EXPERIMENT_DISABLE_IPV6,

    // Probe for IPv6 support.
    HOST_RESOLVER_EXPERIMENT_IPV6_PROBE,

    HOST_RESOLVER_EXPERIMENT_COUNT,
  };

  // The "Experiment" structure describes an individual test to run.
  struct Experiment {
    Experiment(const GURL& url,
               ProxySettingsExperiment proxy_settings_experiment,
               HostResolverExperiment host_resolver_experiment)
        : url(url),
          proxy_settings_experiment(proxy_settings_experiment),
          host_resolver_experiment(host_resolver_experiment) {
    }

    // The URL to try and fetch.
    GURL url;

    // The proxy settings to use.
    ProxySettingsExperiment proxy_settings_experiment;

    // The host resolver settings to use.
    HostResolverExperiment host_resolver_experiment;
  };

  typedef std::vector<Experiment> ExperimentList;

  // "Delegate" is an interface for receiving start and completion notification
  // of individual tests that are run by the ConnectionTester.
  //
  // NOTE: do not delete the ConnectionTester when executing within one of the
  // delegate methods.
  class Delegate {
   public:
    // Called once the test suite is about to start.
    virtual void OnStartConnectionTestSuite() = 0;

    // Called when an individual experiment is about to be started.
    virtual void OnStartConnectionTestExperiment(
        const Experiment& experiment) = 0;

    // Called when an individual experiment has completed.
    //   |experiment| - the experiment that has completed.
    //   |result| - the net error that the experiment completed with
    //              (or net::OK if it was success).
    virtual void OnCompletedConnectionTestExperiment(
        const Experiment& experiment,
        int result) = 0;

    // Called once ALL tests have completed.
    virtual void OnCompletedConnectionTestSuite() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Constructs a ConnectionTester that notifies test progress to |delegate|.
  // |delegate| is owned by the caller, and must remain valid for the lifetime
  // of ConnectionTester.
  ConnectionTester(Delegate* delegate,
                   net::URLRequestContext* proxy_request_context,
                   net::NetLog* net_log);

  // Note that destruction cancels any in-progress tests.
  ~ConnectionTester();

  // Starts running the test suite on |url|. Notification of progress is sent to
  // |delegate_|.
  void RunAllTests(const GURL& url);

  // Returns a text string explaining what |experiment| is testing.
  static base::string16 ProxySettingsExperimentDescription(
      ProxySettingsExperiment experiment);
  static base::string16 HostResolverExperimentDescription(
      HostResolverExperiment experiment);

 private:
  // Internally each experiment run by ConnectionTester is handled by a
  // "TestRunner" instance.
  class TestRunner;
  friend class TestRunner;

  // Fills |list| with the set of all possible experiments for |url|.
  static void GetAllPossibleExperimentCombinations(const GURL& url,
                                                   ExperimentList* list);

  // Starts the next experiment from |remaining_experiments_|.
  void StartNextExperiment();

  // Callback for when |current_test_runner_| finishes.
  void OnExperimentCompleted(int result);

  // Returns the experiment at the front of our list.
  const Experiment& current_experiment() const {
    return remaining_experiments_.front();
  }

  // The object to notify test progress to.
  Delegate* delegate_;

  // The current in-progress test, or NULL if there is no active test.
  scoped_ptr<TestRunner> current_test_runner_;

  // The ordered list of experiments to try next. The experiment at the front
  // of the list is the one currently in progress.
  ExperimentList remaining_experiments_;

  net::URLRequestContext* const proxy_request_context_;

  net::NetLog* net_log_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionTester);
};

#endif  // CHROME_BROWSER_NET_CONNECTION_TESTER_H_
