// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/hyphenator/hyphenator.h"

#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/utf_string_conversions.h"
#include "content/common/hyphenator_messages.h"
#include "content/public/test/mock_render_thread.h"
#include "ipc/ipc_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/hyphen/hyphen.h"

namespace content {
namespace {

// A mock message listener that listens for HyphenatorHost messages. This class
// intercepts a HyphenatorHostMsg_OpenDictionary message sent to an
// IPC::TestSink object and emulates the HyphenatorMessageFilter class.
class MockListener : public IPC::Listener {
 public:
  MockListener(Hyphenator* hyphenator, const string16& locale)
      : hyphenator_(hyphenator),
        locale_(locale) {
  }
  virtual ~MockListener() {
  }

  // IPC::ChannelProxy::MessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    if (message.type() != HyphenatorHostMsg_OpenDictionary::ID)
      return false;

    // Retrieve the locale parameter directly because HyphenatorHost messages
    // are internal messages and unit tests cannot access its member functions,
    // i.e. unit tests cannot call the HyphenatorHostMsg_OpenDictionary::Read
    // function.
    PickleIterator iter(message);
    string16 locale;
    EXPECT_TRUE(message.ReadString16(&iter, &locale));
    EXPECT_EQ(locale_, locale);

    // Open the default dictionary and call the OnControllMessageReceived
    // function with a HyphenatorMsg_SetDictionary message.
    base::FilePath dictionary_path;
    if (!PathService::Get(base::DIR_SOURCE_ROOT, &dictionary_path))
      return false;
    dictionary_path = dictionary_path.AppendASCII("third_party");
    dictionary_path = dictionary_path.AppendASCII("hyphen");
    dictionary_path = dictionary_path.AppendASCII("hyph_en_US.dic");
    base::PlatformFile file = base::CreatePlatformFile(
        dictionary_path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
        NULL, NULL);
    EXPECT_NE(base::kInvalidPlatformFileValue, file);

    IPC::Message response(
        0, HyphenatorMsg_SetDictionary::ID, IPC::Message::PRIORITY_NORMAL);
    IPC::PlatformFileForTransit transit = IPC::GetFileHandleForProcess(
        file, GetHandle(), false);
    IPC::ParamTraits<IPC::PlatformFileForTransit>::Write(&response, transit);
    hyphenator_->OnControlMessageReceived(response);
    base::ClosePlatformFile(file);
    return true;
  }

 private:
  base::ProcessHandle GetHandle() const {
    return base::Process::Current().handle();
  }

  Hyphenator* hyphenator_;
  string16 locale_;
};

}  // namespace

// A unit test for our hyphenator. This class loads a sample hyphenation
// dictionary and hyphenates words.
class HyphenatorTest : public testing::Test {
 public:
  HyphenatorTest() {
  }

  bool Initialize() {
    base::FilePath dictionary_path;
    if (!PathService::Get(base::DIR_SOURCE_ROOT, &dictionary_path))
      return false;
    dictionary_path = dictionary_path.AppendASCII("third_party");
    dictionary_path = dictionary_path.AppendASCII("hyphen");
    dictionary_path = dictionary_path.AppendASCII("hyph_en_US.dic");
    base::PlatformFile file = base::CreatePlatformFile(
        dictionary_path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
        NULL, NULL);
    hyphenator_.reset(new Hyphenator(file));
    return hyphenator_->Initialize();
  }

  // Creates a human-readable hyphenated word. This function inserts '-'
  // characters to all places where we can insert hyphens to improve the
  // readability of this unit test.
  string16 Hyphenate(const string16& word) {
    string16 hyphenated_word(word);
    size_t position = word.length();
    while (position > 0) {
      size_t new_position = hyphenator_->ComputeLastHyphenLocation(word,
                                                                   position);
      EXPECT_LT(new_position, position);
      if (new_position > 0)
        hyphenated_word.insert(new_position, 1, '-');
      position = new_position;
    }
    return hyphenated_word;
  }

  bool OpenDictionary(const string16& locale) {
    hyphenator_.reset(new Hyphenator(base::kInvalidPlatformFileValue));
    thread_.reset(new MockRenderThread());
    listener_.reset(new MockListener(hyphenator_.get(), locale));
    thread_->sink().AddFilter(listener_.get());
    return hyphenator_->Attach(thread_.get(), locale);
  }

  size_t GetMessageCount() const {
    return thread_->sink().message_count();
  }

 private:
  scoped_ptr<Hyphenator> hyphenator_;
  scoped_ptr<MockRenderThread> thread_;
  scoped_ptr<MockListener> listener_;
};

// Verifies that our hyphenator yields the same hyphenated words as the original
// hyphen library does.
TEST_F(HyphenatorTest, HyphenateWords) {
  static const struct {
    const char* input;
    const char* expected;
  } kTestCases[] = {
    { "and", "and" },
    { "concupiscent,", "con-cu-pis-cent," },
    { "evidence.", "ev-i-dence." },
    { "first", "first" },
    { "getting", "get-ting" },
    { "hedgehog", "hedge-hog" },
    { "remarkable", "re-mark-able" },
    { "straightened", "straight-ened" },
    { "undid", "un-did" },
    { "were", "were" },
    { "Simply", "Sim-ply" },
    { "Undone.", "Un-done." },
    { "comfortably", "com-fort-ably"},
    { "declination", "dec-li-na-tion" },
    { "flamingo:", "flamin-go:" },
    { "lination", "lina-tion" },
    { "reciprocity", "rec-i-proc-i-ty" },
    { "throughout", "through-out" },
    { "undid", "un-did" },
    { "undone.", "un-done." },
    { "unnecessary", "un-nec-es-sary" },
  };
  Initialize();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    string16 input = ASCIIToUTF16(kTestCases[i].input);
    string16 expected = ASCIIToUTF16(kTestCases[i].expected);
    EXPECT_EQ(expected, Hyphenate(input));
  }
}

// Verifies that our hyphenator sends a HyphenatorHostMsg_OpenDictionary
// message to ask a browser to open a dictionary. Also, this test verifies that
// our hyphenator can hyphnate words when the Hyphenator::SetDictionary function
// is called.
TEST_F(HyphenatorTest, openDictionary) {
  // Send a HyphenatorHostMsg_OpenDictionary message and verify it is handled by
  // our MockListner class.
  EXPECT_TRUE(OpenDictionary(string16()));
  EXPECT_EQ(0U, GetMessageCount());

  // Verify that we can now hyphenate words. When the MockListener class
  // receives a HyphenatorHostMsg_OpenDictionary message, it calls the
  // OnControlMessageReceived function with a HyphenatorMsg_SetDictionary
  // message. So, the Hyphenate function should be able to hyphenate words now.
  string16 input = ASCIIToUTF16("hyphenation");
  string16 expected = ASCIIToUTF16("hy-phen-ation");
  EXPECT_EQ(expected, Hyphenate(input));
}

}  // namespace content
