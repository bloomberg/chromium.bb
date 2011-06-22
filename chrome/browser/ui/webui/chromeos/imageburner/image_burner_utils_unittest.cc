#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "base/memory/scoped_ptr.h"
#include "imageburner_impl.h"

namespace imageburner {
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;

std::string ValidConfigFile =
    "name=some_name\n"
    "version=version\n"
    "hwid=hwid1\n"
    "hwid=hwid2\n"
    "hwid=hwid3\n"
    "\n"
    "filesize=1000\n"
    "url=http://image.bin.zip";

std::string InvalidConfigFile =
    "name=some_name\n"  // Good line.
    "version=version \n"  // Trailing whitespace.
    "hwid=hwid1=q\n"  // Extra =.
    "hwid=hwid2\n"
    "hwid=  \n"  // Blank property value.
    "\n"
    "filesize=\n"  // Empty property value.
    "url";  // No =.

TEST(ImageBurnerUtilsTest, ConfigFileTest) {
  scoped_ptr<ConfigFile> cf(new ConfigFile());
  EXPECT_TRUE(cf->empty());
  
  cf.reset(new ConfigFile(""));
  EXPECT_TRUE(cf->empty());

  cf.reset(new ConfigFile(ValidConfigFile));
  EXPECT_FALSE(cf->empty());
  EXPECT_EQ("some_name", cf->GetProperty("name"));
  EXPECT_EQ("version", cf->GetProperty("version"));
  EXPECT_EQ("1000", cf->GetProperty("filesize"));
  EXPECT_EQ("http://image.bin.zip", cf->GetProperty("url"));
  EXPECT_EQ("", cf->GetProperty("hwid"));
  EXPECT_EQ("", cf->GetProperty(""));
  EXPECT_EQ("", cf->GetProperty("some_name"));
  EXPECT_TRUE(cf->ContainsHwid("hwid2"));
  EXPECT_TRUE(cf->ContainsHwid("hwid1"));
  EXPECT_FALSE(cf->ContainsHwid("hwid"));

  cf->clear();
  EXPECT_TRUE(cf->empty());

  cf->reset(InvalidConfigFile);
  EXPECT_FALSE(cf->empty());
  EXPECT_EQ("some_name", cf->GetProperty("name"));
 // EXPECT_EQ("version", cf->GetProperty("version"));
  EXPECT_EQ("", cf->GetProperty("filesize"));
  EXPECT_EQ("", cf->GetProperty("url"));
  EXPECT_TRUE(cf->ContainsHwid("hwid2"));
  EXPECT_FALSE(cf->ContainsHwid("hwid1"));
 // EXPECT_FALSE(cf->ContainsHwid("  "));
}

class MockStateMachineObserver : public StateMachine::Observer {
 public:
  MOCK_METHOD1(OnBurnStateChanged, void(StateMachine::State));
  MOCK_METHOD1(OnError, void(int));
};

TEST(ImageBurnerUtilsTest, StateMachineNormalWorkflow) {
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
  
  EXPECT_FALSE(state_machine->image_download_requested());
  EXPECT_FALSE(state_machine->download_started());
  EXPECT_FALSE(state_machine->download_finished());
  EXPECT_TRUE(state_machine->new_burn_posible());
  
  state_machine->OnImageDownloadRequested();
  
  EXPECT_EQ(StateMachine::INITIAL, state_machine->state());
  EXPECT_TRUE(state_machine->image_download_requested());
  EXPECT_FALSE(state_machine->download_started());
  EXPECT_FALSE(state_machine->download_finished());
  EXPECT_TRUE(state_machine->new_burn_posible());

  state_machine->OnDownloadStarted();

  EXPECT_EQ(StateMachine::DOWNLOADING, state_machine->state());
  EXPECT_TRUE(state_machine->image_download_requested());
  EXPECT_TRUE(state_machine->download_started());
  EXPECT_FALSE(state_machine->download_finished());
  EXPECT_FALSE(state_machine->new_burn_posible());
  
  state_machine->OnDownloadFinished();

 // EXPECT_EQ(StateMachine::INITIAL, state_machine->state());
  EXPECT_TRUE(state_machine->image_download_requested());
  EXPECT_TRUE(state_machine->download_started());
  EXPECT_TRUE(state_machine->download_finished());
  EXPECT_FALSE(state_machine->new_burn_posible());
  
  state_machine->OnBurnStarted();

  EXPECT_EQ(StateMachine::BURNING, state_machine->state());
  EXPECT_TRUE(state_machine->image_download_requested());
  EXPECT_TRUE(state_machine->download_started());
  EXPECT_TRUE(state_machine->download_finished());
  EXPECT_FALSE(state_machine->new_burn_posible());
  
  state_machine->OnSuccess();

  EXPECT_EQ(StateMachine::INITIAL, state_machine->state());
  EXPECT_TRUE(state_machine->image_download_requested());
  EXPECT_TRUE(state_machine->download_started());
  EXPECT_TRUE(state_machine->download_finished());
  EXPECT_TRUE(state_machine->new_burn_posible());
}

TEST(ImageBurnerUtilsTest, StateMachineError) {
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

  state_machine->OnImageDownloadRequested();
  state_machine->OnDownloadStarted();

  state_machine->OnError(1234);

  // If called before download finished, download flags should be reset.
  EXPECT_FALSE(state_machine->image_download_requested());
  EXPECT_FALSE(state_machine->download_started());
  EXPECT_EQ(state_machine->state(), StateMachine::INITIAL);
  EXPECT_TRUE(state_machine->new_burn_posible());

  state_machine->OnImageDownloadRequested();
  state_machine->OnDownloadStarted();
  state_machine->OnDownloadFinished();

  state_machine->OnError(4321);

  // If called after download finished, download flags should not be changed.
  EXPECT_TRUE(state_machine->image_download_requested());
  EXPECT_TRUE(state_machine->download_started());
  EXPECT_TRUE(state_machine->download_finished());
  EXPECT_EQ(state_machine->state(), StateMachine::INITIAL);
  EXPECT_TRUE(state_machine->new_burn_posible());

  state_machine->OnBurnStarted();
  state_machine->OnError(0);

  EXPECT_EQ(state_machine->state(), StateMachine::INITIAL);
  EXPECT_TRUE(state_machine->new_burn_posible());
}

TEST(ImageBurnerUtilsTest, StateaAchineCancelation) {
  scoped_ptr<StateMachine> state_machine(new StateMachine());  

  MockStateMachineObserver observer;
  EXPECT_CALL(observer, OnBurnStateChanged(StateMachine::INITIAL))
      .Times(0);
  EXPECT_CALL(observer, OnBurnStateChanged(StateMachine::BURNING))
      .Times(1);
  EXPECT_CALL(observer, OnBurnStateChanged(StateMachine::DOWNLOADING))
      .Times(1);
  EXPECT_CALL(observer, OnBurnStateChanged(StateMachine::CANCELLED))
      .Times(3);
  state_machine->AddObserver(&observer);

  state_machine->OnCancelation();  
  EXPECT_EQ(StateMachine::INITIAL, state_machine->state());

  // Let's change state to DOWNLOADING.
  state_machine->OnDownloadStarted();
  EXPECT_EQ(StateMachine::DOWNLOADING, state_machine->state());

  state_machine->OnCancelation();

  EXPECT_EQ(StateMachine::DOWNLOADING, state_machine->state());
  
  // Let's change state to BURNING.
  state_machine->OnBurnStarted();
  EXPECT_EQ(StateMachine::BURNING, state_machine->state());

  state_machine->OnCancelation();

  EXPECT_EQ(StateMachine::BURNING, state_machine->state());
}

TEST(ImageBurnerUtilsTest, StateMachineObservers) {
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

MockDownloaderListener :: Downloader::Listener {
 public:
  MOCK_METHOD_1(OnBurnStarted, void(bool));
};

class DownloaderTest : public testing::Test {
 public:


 protected:
  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
  BrowserThread download_thread_;

}


}




