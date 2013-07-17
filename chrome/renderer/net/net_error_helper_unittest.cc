// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper.h"

#include "base/logging.h"
#include "chrome/common/net/net_error_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome_common_net::DnsProbeStatus;
using chrome_common_net::DnsProbeStatusToString;

// NetErrorHelperTest cases consist of a string of these steps.
enum TestStep {
  // Simulate a provisional load start, fail, commit or a finish-load event.
  // (Start and fail differentiate between normal and error pages.)
  LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START,
  LOAD_COMMIT, LOAD_FINISH,

  // Simulate an IPC from the browser with DNS_PROBE_STARTED, _NOT_RUN, or
  // _FINISHED_NXDOMAIN.
  STATUS_STARTED, STATUS_NOT_RUN, STATUS_FINISHED,

  // Expect that the *next* step will cause an update.  (Any step that is not
  // prefixed by this pseudo-step is expected *not* to cause an update.)
  EXPECT_UPDATE
};

class TestNetErrorHelper : public NetErrorHelper {
 public:
  TestNetErrorHelper()
      : NetErrorHelper(NULL),
        mock_page_update_count_(0),
        mock_displayed_probe_status_(chrome_common_net::DNS_PROBE_MAX) {}

  virtual ~TestNetErrorHelper() {}

  void StartLoad(bool is_main_frame, bool is_error_page) {
    OnStartLoad(is_main_frame, is_error_page);
  }

  void FailLoad(bool is_main_frame, bool is_dns_error) {
    OnFailLoad(is_main_frame, is_dns_error);
  }

  void CommitLoad(bool is_main_frame) {
    OnCommitLoad(is_main_frame);
  }

  void FinishLoad(bool is_main_frame) {
    OnFinishLoad(is_main_frame);
  }

  void ReceiveProbeStatus(DnsProbeStatus status) {
    OnNetErrorInfo(static_cast<int>(status));
  }

  int mock_page_update_count() const { return mock_page_update_count_;  }
  DnsProbeStatus mock_displayed_probe_status() const {
    return mock_displayed_probe_status_;
  }

 protected:
  virtual void UpdateErrorPage() OVERRIDE {
    DVLOG(1) << "Updating error page with status "
             << DnsProbeStatusToString(last_probe_status_);
    mock_page_update_count_++;
    mock_displayed_probe_status_ = last_probe_status_;
  }

 private:
  int mock_page_update_count_;
  DnsProbeStatus mock_displayed_probe_status_;
};

class NetErrorHelperTest : public testing::Test {
 protected:
  enum MainFrame { SUB_FRAME, MAIN_FRAME };
  enum ErrorPage { NORMAL_PAGE, ERROR_PAGE };
  enum ErrorType { OTHER_ERROR, DNS_ERROR };

  void StartLoad(MainFrame main_frame, ErrorPage error_page) {
    helper_.StartLoad(main_frame == MAIN_FRAME, error_page == ERROR_PAGE);
  }

  void FailLoad(MainFrame main_frame, ErrorType error_type) {
    helper_.FailLoad(main_frame == MAIN_FRAME, error_type == DNS_ERROR);
  }

  void CommitLoad(MainFrame main_frame) {
    helper_.CommitLoad(main_frame == MAIN_FRAME);
  }

  void FinishLoad(MainFrame main_frame) {
    helper_.FinishLoad(main_frame == MAIN_FRAME);
  }

  void ReceiveProbeStatus(DnsProbeStatus status) {
    helper_.ReceiveProbeStatus(status);
  }

  void RunTest(const TestStep steps[], int step_count);

  int page_update_count() const { return helper_.mock_page_update_count(); }
  DnsProbeStatus displayed_probe_status() const {
    return helper_.mock_displayed_probe_status();
  }

 private:
  TestNetErrorHelper helper_;
};

void NetErrorHelperTest::RunTest(const TestStep steps[], int step_count) {
  // Whether the next instruction is expected to cause an update (since the
  // step right before it was EXPECT_UPDATE) or not.
  bool update_expected = false;
  int expected_update_count = page_update_count();
  // The last status that the test simulated receiving from the browser.
  // When an update is expected, the status is expected to match this.
  chrome_common_net::DnsProbeStatus last_status_received =
      chrome_common_net::DNS_PROBE_POSSIBLE;

  for (int i = 0; i < step_count; i++) {
    switch (steps[i]) {
    case LOAD_NORMAL_START:
      StartLoad(MAIN_FRAME, NORMAL_PAGE);
      break;
    case LOAD_NORMAL_FAIL:
      FailLoad(MAIN_FRAME, DNS_ERROR);
      break;
    case LOAD_ERROR_START:
      StartLoad(MAIN_FRAME, ERROR_PAGE);
      break;
    case LOAD_COMMIT:
      CommitLoad(MAIN_FRAME);
      break;
    case LOAD_FINISH:
      FinishLoad(MAIN_FRAME);
      break;
    case STATUS_STARTED:
      ReceiveProbeStatus(chrome_common_net::DNS_PROBE_STARTED);
      last_status_received = chrome_common_net::DNS_PROBE_STARTED;
      break;
    case STATUS_NOT_RUN:
      ReceiveProbeStatus(chrome_common_net::DNS_PROBE_NOT_RUN);
      last_status_received = chrome_common_net::DNS_PROBE_NOT_RUN;
      break;
    case STATUS_FINISHED:
      ReceiveProbeStatus(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
      last_status_received = chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN;
      break;
    case EXPECT_UPDATE:
      ASSERT_FALSE(update_expected);
      update_expected = true;
      // Skip to next step to see if it updates the status, instead of
      // checking whether EXPECT_UPDATE itself caused an update.
      continue;
    }

    if (update_expected) {
      DCHECK_NE(chrome_common_net::DNS_PROBE_POSSIBLE, last_status_received);
      ++expected_update_count;

      EXPECT_EQ(last_status_received, displayed_probe_status());
      if (displayed_probe_status() != last_status_received) {
        LOG(ERROR) << "Unexpected status at step " << i << ".";
        return;
      }
    }

    EXPECT_EQ(expected_update_count, page_update_count());
    if (page_update_count() != expected_update_count) {
      LOG(ERROR) << (update_expected ? "Missing" : "Spurious")
                 << " update at step " << i << ".";
      return;
    }

    update_expected = false;
  }

  DCHECK(!update_expected);
}

TEST_F(NetErrorHelperTest, Null) {
  // Test that we can simply create and destroy a NetErrorHelper.
}

TEST_F(NetErrorHelperTest, SuccessfulPageLoad) {
  StartLoad(MAIN_FRAME, NORMAL_PAGE);
  CommitLoad(MAIN_FRAME);
  FinishLoad(MAIN_FRAME);

  // Ignore spurious status.
  ReceiveProbeStatus(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, page_update_count());
}

TEST_F(NetErrorHelperTest, MainFrameNonDnsError) {
  StartLoad(MAIN_FRAME, NORMAL_PAGE);
  FailLoad(MAIN_FRAME, OTHER_ERROR);
  StartLoad(MAIN_FRAME, ERROR_PAGE);
  CommitLoad(MAIN_FRAME);
  FinishLoad(MAIN_FRAME);

  // Ignore spurious status.
  ReceiveProbeStatus(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, page_update_count());
}

TEST_F(NetErrorHelperTest, SubFrameDnsError) {
  StartLoad(SUB_FRAME, NORMAL_PAGE);
  FailLoad(SUB_FRAME, DNS_ERROR);
  StartLoad(SUB_FRAME, ERROR_PAGE);
  CommitLoad(SUB_FRAME);
  FinishLoad(SUB_FRAME);

  // Ignore spurious status.
  ReceiveProbeStatus(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, page_update_count());
}

TEST_F(NetErrorHelperTest, FinishedAfterFail) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, STATUS_FINISHED, LOAD_ERROR_START,
    LOAD_COMMIT, EXPECT_UPDATE, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterFail_StartedAfterFail) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, STATUS_STARTED, STATUS_FINISHED,
    LOAD_ERROR_START, LOAD_COMMIT, EXPECT_UPDATE, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterStart) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, STATUS_FINISHED,
    LOAD_COMMIT, EXPECT_UPDATE, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterStart_StartedAfterFail) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, STATUS_STARTED, LOAD_ERROR_START,
    STATUS_FINISHED, LOAD_COMMIT, EXPECT_UPDATE, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterStart_StartedAfterStart) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, STATUS_STARTED, LOAD_ERROR_START,
    STATUS_FINISHED, LOAD_COMMIT, EXPECT_UPDATE, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterCommit) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, LOAD_COMMIT,
    STATUS_FINISHED, EXPECT_UPDATE, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterCommit_StartedAfterFail) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, STATUS_STARTED, LOAD_ERROR_START,
    LOAD_COMMIT, STATUS_FINISHED, EXPECT_UPDATE, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterCommit_StartedAfterStart) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, STATUS_STARTED,
    LOAD_ERROR_START, LOAD_COMMIT, STATUS_FINISHED, EXPECT_UPDATE, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterCommit_StartedAfterCommit) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, LOAD_COMMIT,
    STATUS_STARTED, STATUS_FINISHED, EXPECT_UPDATE, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterFinish) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, LOAD_COMMIT,
    LOAD_FINISH, EXPECT_UPDATE, STATUS_FINISHED
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterFinish_StartAfterFail) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, STATUS_STARTED, LOAD_ERROR_START,
    LOAD_COMMIT, EXPECT_UPDATE, LOAD_FINISH, EXPECT_UPDATE, STATUS_FINISHED
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterFinish_StartAfterStart) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, STATUS_STARTED,
    LOAD_COMMIT, EXPECT_UPDATE, LOAD_FINISH, EXPECT_UPDATE, STATUS_FINISHED
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterFinish_StartAfterCommit) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, LOAD_COMMIT,
    STATUS_STARTED, EXPECT_UPDATE, LOAD_FINISH, EXPECT_UPDATE, STATUS_FINISHED
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterFinish_StartAfterFinish) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, LOAD_COMMIT,
    LOAD_FINISH, EXPECT_UPDATE, STATUS_STARTED, EXPECT_UPDATE, STATUS_FINISHED
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterNewStart) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, LOAD_COMMIT,
    LOAD_FINISH, LOAD_NORMAL_START, EXPECT_UPDATE, STATUS_FINISHED,
    LOAD_COMMIT, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, NotRunAfterFail) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, STATUS_NOT_RUN, LOAD_ERROR_START,
    LOAD_COMMIT, EXPECT_UPDATE, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, NotRunAfterStart) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, STATUS_NOT_RUN,
    LOAD_COMMIT, EXPECT_UPDATE, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, NotRunAfterCommit) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, LOAD_COMMIT,
    STATUS_NOT_RUN, EXPECT_UPDATE, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, NotRunAfterFinish) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, LOAD_COMMIT,
    LOAD_FINISH, EXPECT_UPDATE, STATUS_NOT_RUN
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterNewCommit) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, LOAD_COMMIT,
    LOAD_FINISH, LOAD_NORMAL_START, LOAD_COMMIT, STATUS_FINISHED, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

TEST_F(NetErrorHelperTest, FinishedAfterNewFinish) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, LOAD_COMMIT,
    LOAD_FINISH, LOAD_NORMAL_START, LOAD_COMMIT, LOAD_FINISH, STATUS_FINISHED
  };
  RunTest(steps, arraysize(steps));
}

// Two iterations of FinishedAfterStart_StartAfterFail
TEST_F(NetErrorHelperTest, TwoProbes_FinishedAfterStart_StartAfterFail) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, STATUS_STARTED, LOAD_ERROR_START,
    STATUS_FINISHED, LOAD_COMMIT, EXPECT_UPDATE, LOAD_FINISH,
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, STATUS_STARTED, LOAD_ERROR_START,
    STATUS_FINISHED, LOAD_COMMIT, EXPECT_UPDATE, LOAD_FINISH
  };
  RunTest(steps, arraysize(steps));
}

// Two iterations of FinishedAfterFinish
TEST_F(NetErrorHelperTest, TwoProbes_FinishedAfterFinish) {
  const TestStep steps[] = {
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, LOAD_COMMIT,
    LOAD_FINISH, EXPECT_UPDATE, STATUS_FINISHED,
    LOAD_NORMAL_START, LOAD_NORMAL_FAIL, LOAD_ERROR_START, LOAD_COMMIT,
    LOAD_FINISH, EXPECT_UPDATE, STATUS_FINISHED
  };
  RunTest(steps, arraysize(steps));
}
