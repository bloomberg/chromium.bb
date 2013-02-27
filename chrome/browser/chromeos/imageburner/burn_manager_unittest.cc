// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/imageburner/burn_manager.h"

namespace chromeos {
namespace imageburner {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;

const std::string kConfigFileWithNoHwidProperty =
    "name=some_name\n"
    "version=version\n"
    "filesize=1000\n"
    "url=http://image.bin.zip\n";

const std::string kConfigFileWithNoNameProperty =
    "version=version\n"
    "filesize=2000\n"
    "url=http://some_image.bin.zip\n";

const std::string kConfigFileWithNoNewLineAtEnd =
    "name=some_name\n"
    "version=version\n"
    "filesize=1000\n"
    "hwid=some_hwid\n"
    "url=http://image.bin.zip";

const std::string kSampleConfigFile =
    "version=aaa\n"
    "hwid=block_no_name\n"
    "url=aaa\n"
    "\n"
    "name=some_name1\n"
    "version=version1\n"
    "hwid=hwid11\n"
    "hwid=hwid12\n"
    "hwid=hwid13\n"
    "\n"
    "filesize=1000\n"
    "url=http://image1.bin.zip\n"
    "file=url\n"
    "name=some_name2\n"
    "version=version2\n"
    "hwid=hwid21\n"
    "hwid=hwid22\n"
    "hwid=hwid23\n"
    "\n"
    "filesize=1200\n"
    "url=http://image2.bin.zip\n"
    "file=file2"
    "\n"
    "name=some_name3\n"
    "version=version3\n"
    "hwid=hwid31\n"
    "\n"
    "filesize=3\n"
    "url=http://image3.bin.zip\n"
    "file=file3"
    "\n"
    "name=some_block_with_no_hwid\n"
    "url=some_url\n"
    "\n"
    "name=some_name_invalid_block\n"  // Good line.
    "version=version \n"  // Trailing whitespace.
    "hwid=hwid41=q\n"  // Extra =.
    "hwid=hwid42\n"
    "hwid=  \n"  // Blank property value.
    "=\n"
    "filesize=\n"  // Empty property value.
    "url\n"  // No =.
    "   =something\n"
    "name=another_block_with_no_hwid\n"
    "version=version\n";

TEST(BurnManagerTest, ConfigFileTest) {
  scoped_ptr<ConfigFile> cf(new ConfigFile());
  EXPECT_TRUE(cf->empty());

  cf.reset(new ConfigFile(""));
  EXPECT_TRUE(cf->empty());

  cf.reset(new ConfigFile(kConfigFileWithNoNameProperty));
  EXPECT_TRUE(cf->empty());

  cf.reset(new ConfigFile(kConfigFileWithNoHwidProperty));
  EXPECT_TRUE(cf->empty());

  cf.reset(new ConfigFile(kConfigFileWithNoNewLineAtEnd));
  EXPECT_FALSE(cf->empty());
  EXPECT_EQ(1u, cf->size());
  EXPECT_EQ("http://image.bin.zip", cf->GetProperty("url", "some_hwid"));
  EXPECT_EQ("some_name", cf->GetProperty("name", "some_hwid"));

  cf.reset(new ConfigFile(kSampleConfigFile));
  EXPECT_FALSE(cf->empty());

  EXPECT_EQ(4u, cf->size());

  EXPECT_EQ("", cf->GetProperty("version", "block_no_name"));

  EXPECT_EQ("some_name1", cf->GetProperty("name", "hwid11"));
  EXPECT_EQ("version1", cf->GetProperty("version", "hwid12"));
  EXPECT_EQ("", cf->GetProperty("filesize", "hwid1_non_existent"));
  EXPECT_EQ("http://image1.bin.zip", cf->GetProperty("url", "hwid13"));
  EXPECT_EQ("", cf->GetProperty("hwid", "hwid11"));
  EXPECT_EQ("", cf->GetProperty("", "hwid12"));
  EXPECT_EQ("", cf->GetProperty("name", ""));
  EXPECT_EQ("", cf->GetProperty("some_name", "hwid11"));
  EXPECT_EQ("url", cf->GetProperty("file", "hwid11"));

  EXPECT_EQ("http://image2.bin.zip", cf->GetProperty("url", "hwid21"));
  EXPECT_EQ("some_name2", cf->GetProperty("name", "hwid23"));

  EXPECT_EQ("http://image3.bin.zip", cf->GetProperty("url", "hwid31"));
  EXPECT_EQ("some_name3", cf->GetProperty("name", "hwid31"));

  EXPECT_EQ("some_name_invalid_block", cf->GetProperty("name", "hwid42"));
  // TODO(tbarzic): make this pass.
  // EXPECT_EQ("version", cf->GetProperty("version", "hwid42"));
  EXPECT_EQ("", cf->GetProperty("filesize", "hwid42"));
  EXPECT_EQ("", cf->GetProperty("url", "hwid42"));
  // TODO(tbarzic): make this pass.
  // EXPECT_EQ("", cf->GetProperty("  ", "hwid42"));
  EXPECT_EQ("", cf->GetProperty("name", "hwid41"));
}

class MockStateMachineObserver : public StateMachine::Observer {
 public:
  MOCK_METHOD1(OnBurnStateChanged, void(StateMachine::State));
  MOCK_METHOD1(OnError, void(int));
};

TEST(BurnManagerTest, StateMachineNormalWorkflow) {
  scoped_ptr<StateMachine> state_machine(new StateMachine());
  EXPECT_EQ(StateMachine::INITIAL, state_machine->state());

  MockStateMachineObserver observer;
  state_machine->AddObserver(&observer);
  EXPECT_CALL(observer, OnBurnStateChanged(StateMachine::DOWNLOADING))
    .Times(1)
    .RetiresOnSaturation();

  EXPECT_CALL(observer, OnBurnStateChanged(StateMachine::BURNING))
    .Times(1)
    .RetiresOnSaturation();

  EXPECT_CALL(observer, OnBurnStateChanged(StateMachine::INITIAL))
    .Times(1)
    .RetiresOnSaturation();

  EXPECT_FALSE(state_machine->download_started());
  EXPECT_FALSE(state_machine->download_finished());
  EXPECT_TRUE(state_machine->new_burn_posible());

  state_machine->OnDownloadStarted();

  EXPECT_EQ(StateMachine::DOWNLOADING, state_machine->state());
  EXPECT_TRUE(state_machine->download_started());
  EXPECT_FALSE(state_machine->download_finished());
  EXPECT_FALSE(state_machine->new_burn_posible());

  state_machine->OnDownloadFinished();

  // TODO(tbarzic): make this pass.
  // EXPECT_EQ(StateMachine::INITIAL, state_machine->state());
  EXPECT_TRUE(state_machine->download_started());
  EXPECT_TRUE(state_machine->download_finished());
  EXPECT_FALSE(state_machine->new_burn_posible());

  state_machine->OnBurnStarted();

  EXPECT_EQ(StateMachine::BURNING, state_machine->state());
  EXPECT_TRUE(state_machine->download_started());
  EXPECT_TRUE(state_machine->download_finished());
  EXPECT_FALSE(state_machine->new_burn_posible());

  state_machine->OnSuccess();

  EXPECT_EQ(StateMachine::INITIAL, state_machine->state());
  EXPECT_TRUE(state_machine->download_started());
  EXPECT_TRUE(state_machine->download_finished());
  EXPECT_TRUE(state_machine->new_burn_posible());
}

TEST(BurnManagerTest, StateMachineError) {
  scoped_ptr<StateMachine> state_machine(new StateMachine());

  MockStateMachineObserver observer;
  // We don't want state change to INITIAL due to error to be reported to
  // observers. We use OnError for that.
  EXPECT_CALL(observer, OnBurnStateChanged(_))
      .Times(AnyNumber());
  EXPECT_CALL(observer, OnBurnStateChanged(StateMachine::INITIAL))
      .Times(0);
  {
    InSequence error_calls;
    EXPECT_CALL(observer, OnError(1234))
        .Times(1);
    EXPECT_CALL(observer, OnError(4321))
        .Times(1);
    EXPECT_CALL(observer, OnError(0))
        .Times(1);
  }
  state_machine->AddObserver(&observer);

  state_machine->OnDownloadStarted();

  state_machine->OnError(1234);

  // If called before download finished, download flags should be reset.
  EXPECT_FALSE(state_machine->download_started());
  EXPECT_EQ(state_machine->state(), StateMachine::INITIAL);
  EXPECT_TRUE(state_machine->new_burn_posible());

  state_machine->OnDownloadStarted();
  state_machine->OnDownloadFinished();

  state_machine->OnError(4321);

  // If called after download finished, download flags should not be changed.
  EXPECT_TRUE(state_machine->download_started());
  EXPECT_TRUE(state_machine->download_finished());
  EXPECT_EQ(state_machine->state(), StateMachine::INITIAL);
  EXPECT_TRUE(state_machine->new_burn_posible());

  state_machine->OnBurnStarted();
  state_machine->OnError(0);

  EXPECT_EQ(state_machine->state(), StateMachine::INITIAL);
  EXPECT_TRUE(state_machine->new_burn_posible());
}

TEST(BurnManagerTest, StateMachineObservers) {
  scoped_ptr<StateMachine> state_machine(new StateMachine());

  MockStateMachineObserver observer1, observer2;

  EXPECT_CALL(observer1, OnBurnStateChanged(_))
      .Times(0);
  EXPECT_CALL(observer2, OnBurnStateChanged(_))
      .Times(0);
  EXPECT_CALL(observer1, OnError(_))
      .Times(0);
  EXPECT_CALL(observer2, OnError(_))
      .Times(0);

  state_machine->OnDownloadStarted();
  state_machine->OnError(1);

  state_machine->AddObserver(&observer1);
  state_machine->AddObserver(&observer2);
  EXPECT_CALL(observer1, OnBurnStateChanged(_))
      .Times(1);
  EXPECT_CALL(observer2, OnBurnStateChanged(_))
      .Times(1);
  EXPECT_CALL(observer1, OnError(_))
      .Times(1);
  EXPECT_CALL(observer2, OnError(_))
      .Times(1);

  state_machine->OnDownloadStarted();
  state_machine->OnError(1);

  state_machine->RemoveObserver(&observer1);
  EXPECT_CALL(observer1, OnBurnStateChanged(_))
      .Times(0);
  EXPECT_CALL(observer2, OnBurnStateChanged(_))
      .Times(1);
  EXPECT_CALL(observer1, OnError(_))
      .Times(0);
  EXPECT_CALL(observer2, OnError(_))
      .Times(1);
  state_machine->OnDownloadStarted();
  state_machine->OnError(1);
}

}  // namespace imageburner
}  // namespace chromeos
