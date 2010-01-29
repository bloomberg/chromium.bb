// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/stl_util-inl.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "chrome/browser/net/test_url_fetcher_factory.h"
#include "chrome/browser/renderer_host/translation_service.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_message.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef std::vector<string16> TextChunks;
typedef std::vector<TextChunks> TextChunksList;

class TestMessageSender : public IPC::Message::Sender {
 public:
  virtual ~TestMessageSender() {
    ClearMessages();
  }

  virtual bool Send(IPC::Message* msg) {
    messages_.push_back(msg);
    return true;
  }

  size_t message_count() const { return messages_.size(); }

  void GetMessageParameters(size_t message_index, int* routing_id,
                            int* work_id, int* error_id,
                            std::vector<string16>* text_chunks) {
    ASSERT_LT(message_index, messages_.size());
    *routing_id = messages_.at(message_index)->routing_id();
    ViewMsg_TranslateTextReponse::Read(messages_.at(message_index),
                                       work_id, error_id, text_chunks);
  }

  void ClearMessages() {
    STLDeleteElements(&messages_);
    messages_.clear();
  }

  std::vector<IPC::Message*> messages_;
  MessageLoop message_loop_;
};

class TestTranslationService : public TranslationService {
 public:
  explicit TestTranslationService(IPC::Message::Sender* message_sender)
      : TranslationService(message_sender) {}

  virtual int GetSendRequestDelay() const {
    return 0;
  }
};

class TranslationServiceTest : public testing::Test {
 public:
  TranslationServiceTest() : translation_service_(&message_sender_) {}

  virtual void SetUp() {
    URLFetcher::set_factory(&url_fetcher_factory_);
  }

  virtual void TearDown() {
    URLFetcher::set_factory(NULL);
  }

 protected:
  TestURLFetcherFactory url_fetcher_factory_;
  TestMessageSender message_sender_;
  TestTranslationService translation_service_;
};

static void SimulateErrorResponse(TestURLFetcher* url_fetcher,
                                  int response_code) {
  url_fetcher->delegate()->OnURLFetchComplete(url_fetcher,
                                              url_fetcher->original_url(),
                                              URLRequestStatus(),
                                              response_code,
                                              ResponseCookies(),
                                              std::string());
}

// Merges the strings in |text_chunks| into the string that the translation
// server received.
static string16 BuildResponseString(const TextChunks& text_chunks) {
  string16 text;
  for (size_t i = 0; i < text_chunks.size(); ++i) {
    text.append(ASCIIToUTF16("<a _CR_TR_ id='"));
    text.append(IntToString16(i));
    text.append(ASCIIToUTF16("'>"));
    text.append(text_chunks.at(i));
    text.append(ASCIIToUTF16("</a>"));
  }
  return text;
}

static void SimulateSuccessfulResponse(TestURLFetcher* url_fetcher,
                                       const TextChunksList& text_chunks_list) {
  scoped_ptr<ListValue> list(new ListValue());
  for (TextChunksList::const_iterator iter = text_chunks_list.begin();
       iter != text_chunks_list.end(); ++iter) {
    list->Append(Value::CreateStringValueFromUTF16(BuildResponseString(*iter)));
  }

  std::string response;
  base::JSONWriter::Write(list.get(), false, &response);
  url_fetcher->delegate()->OnURLFetchComplete(url_fetcher,
                                              url_fetcher->original_url(),
                                              URLRequestStatus(),
                                              200,
                                              ResponseCookies(),
                                              response);
}

static void SimulateSimpleSuccessfulResponse(TestURLFetcher* url_fetcher,
                                             const char* const* c_text_chunks,
                                             size_t text_chunks_length) {
  TextChunks text_chunks;
  for (size_t i = 0; i < text_chunks_length; i++)
    text_chunks.push_back(ASCIIToUTF16(c_text_chunks[i]));

  string16 text = BuildResponseString(text_chunks);

  std::string response;
  scoped_ptr<Value> json_text(Value::CreateStringValueFromUTF16(text));
  base::JSONWriter::Write(json_text.get(), false, &response);
  url_fetcher->delegate()->OnURLFetchComplete(url_fetcher,
                                              url_fetcher->original_url(),
                                              URLRequestStatus(),
                                              200,
                                              ResponseCookies(),
                                              response);
}

// Parses the upload data from |url_fetcher| and puts the value for the q
// parameters in |text_chunks|.
static void ExtractQueryStringsFromUploadData(TestURLFetcher* url_fetcher,
                                              TextChunks* text_chunks) {
  std::string upload_data = url_fetcher->upload_data();

  CStringTokenizer str_tok(upload_data.c_str(), upload_data.c_str() +
                              upload_data.length(), "&");
  while (str_tok.GetNext()) {
    std::string tok = str_tok.token();
    if (tok.size() > 1U && tok.at(0) == 'q' && tok.at(1) == '=')
      text_chunks->push_back(UnescapeForHTML(ASCIIToUTF16(tok.substr(2))));
  }
}

TEST_F(TranslationServiceTest, MergeTestChunks) {
  std::vector<string16> input;
  input.push_back(ASCIIToUTF16("Hello"));
  string16 result = TranslationService::MergeTextChunks(input);
  EXPECT_EQ(ASCIIToUTF16("Hello"), result);
  input.push_back(ASCIIToUTF16(" my name"));
  input.push_back(ASCIIToUTF16(" is"));
  input.push_back(ASCIIToUTF16(" Jay."));
  result = TranslationService::MergeTextChunks(input);
  EXPECT_EQ(ASCIIToUTF16("<a _CR_TR_ id='0'>Hello</a>"
                         "<a _CR_TR_ id='1'> my name</a>"
                         "<a _CR_TR_ id='2'> is</a>"
                         "<a _CR_TR_ id='3'> Jay.</a>"),
                         result);
}

TEST_F(TranslationServiceTest, RemoveTag) {
  const char* kInputs[] = {
    "", "Hello", "<a ></a>", " <a href='http://www.google.com'> Link </a>",
    "<a >Link", "<a link</a>", "<a id=1><a id=1>broken</a></a>",
    "<a id=1>broken</a></a> bad bad</a>",
  };
  const char* kExpected[] = {
    "", "Hello", "", "  Link ", "Link", "<a link", "broken", "broken bad bad"
  };

  ASSERT_EQ(arraysize(kInputs), arraysize(kExpected));
  for (size_t i = 0; i < arraysize(kInputs); ++i) {
    SCOPED_TRACE(::testing::Message::Message() << "Iteration " << i);
    string16 input = ASCIIToUTF16(kInputs[i]);
    string16 output = TranslationService::RemoveTag(input);
    EXPECT_EQ(ASCIIToUTF16(kExpected[i]), output);
  }
}

// Tests that we deal correctly with the various results the translation server
// can return, including the buggy ones.
TEST_F(TranslationServiceTest, SplitTestChunks) {
  // Simple case.
  std::vector<string16> text_chunks;
  TranslationService::SplitTextChunks(ASCIIToUTF16("Hello"), &text_chunks);
  ASSERT_EQ(1U, text_chunks.size());
  EXPECT_EQ(ASCIIToUTF16("Hello"), text_chunks[0]);

  text_chunks.clear();

  // Multiple chunks case, correct syntax.
  TranslationService::SplitTextChunks(
      ASCIIToUTF16("<a _CR_TR_ id='0'>Bonjour</a>"
                   "<a _CR_TR_ id='1'> mon nom</a>"
                   "<a _CR_TR_ id='2'> est</a>"
                   "<a _CR_TR_ id='3'> Jay.</a>"), &text_chunks);
  ASSERT_EQ(4U, text_chunks.size());
  EXPECT_EQ(ASCIIToUTF16("Bonjour"), text_chunks[0]);
  EXPECT_EQ(ASCIIToUTF16(" mon nom"), text_chunks[1]);
  EXPECT_EQ(ASCIIToUTF16(" est"), text_chunks[2]);
  EXPECT_EQ(ASCIIToUTF16(" Jay."), text_chunks[3]);
  text_chunks.clear();

  // Multiple chunks case, duplicate and unbalanced tags.
  // For info, original input:
  // <a _CR_TRANSLATE_ id='0'> Experience </a><a _CR_TRANSLATE_ id='1'>Nexus One
  // </a><a _CR_TRANSLATE_ id='2'>, the new Android phone from Google</a>
  TranslationService::SplitTextChunks(
      ASCIIToUTF16("<a _CR_TR_ id='0'>Experience</a> <a _CR_TR_ id='1'>Nexus"
                   "<a _CR_TR_ id='2'> One,</a></a> <a _CR_TR_ id='2'>the new "
                   "Android Phone</a>"), &text_chunks);
  ASSERT_EQ(3U, text_chunks.size());
  EXPECT_EQ(ASCIIToUTF16("Experience "), text_chunks[0]);
  EXPECT_EQ(ASCIIToUTF16("Nexus One, "), text_chunks[1]);
  EXPECT_EQ(ASCIIToUTF16("the new Android Phone"), text_chunks[2]);
  text_chunks.clear();
}

// Tests that a successful translate works as expected.
TEST_F(TranslationServiceTest, SimpleSuccessfulTranslation) {
  const char* const kEnglishTextChunks[] = {
      "An atom is talking to another atom:",
      "- I think I lost an electron.",
      "- Are you sure?",
      "- I am positive."
  };

  const char* const kFrenchTextChunks[] = {
      "Un atome parle a un autre atome:",
      "- Je crois que j'ai perdu un electron.",
      "- T'es sur?",
      "- Je suis positif."  // Note that the joke translates poorly in French.
  };

  // Translate some text unsecurely.
  std::vector<string16> text_chunks;
  for (size_t i = 0; i < arraysize(kEnglishTextChunks); ++i)
    text_chunks.push_back(ASCIIToUTF16(kEnglishTextChunks[i]));
  translation_service_.Translate(0, 0, 0, text_chunks, "en", "fr", false);

  TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
  // The message was too short to triger the sending of a request to the
  // translation request. A task has been pushed for that.
  EXPECT_TRUE(url_fetcher == NULL);
  MessageLoop::current()->RunAllPending();

  // Now the task has been run, the message should have been sent.
  url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(url_fetcher != NULL);
  // Check the URL is HTTP.
  EXPECT_FALSE(url_fetcher->original_url().SchemeIsSecure());

  // Let's simulate the JSON response from the server.
  SimulateSimpleSuccessfulResponse(url_fetcher, &(kFrenchTextChunks[0]),
                                   arraysize(kFrenchTextChunks));

  // This should have triggered a ViewMsg_TranslateTextReponse message.
  ASSERT_EQ(1U, message_sender_.message_count());

  // Test the message has the right translation.
  int routing_id = 0;
  int work_id = 0;
  int error_id = 0;
  std::vector<string16> translated_text_chunks;
  message_sender_.GetMessageParameters(0, &routing_id, &work_id, &error_id,
                                       &translated_text_chunks);
  EXPECT_EQ(0, routing_id);
  EXPECT_EQ(0, error_id);
  ASSERT_EQ(arraysize(kFrenchTextChunks), translated_text_chunks.size());
  for (size_t i = 0; i < arraysize(kFrenchTextChunks); ++i)
    EXPECT_EQ(ASCIIToUTF16(kFrenchTextChunks[i]), translated_text_chunks[i]);
}

// Tests that on failure we send the expected error message.
TEST_F(TranslationServiceTest, FailedTranslation) {
  std::vector<string16> text_chunks;
  text_chunks.push_back(ASCIIToUTF16("Hello"));
  translation_service_.Translate(0, 0, 0, text_chunks, "en", "fr", false);

  // Run the task that creates the URLFetcher and sends the request.
  MessageLoop::current()->RunAllPending();

  TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(url_fetcher != NULL);
  SimulateErrorResponse(url_fetcher, 500);

  // This should have triggered a ViewMsg_TranslateTextReponse message.
  ASSERT_EQ(1U, message_sender_.message_count());

  // Test the message has some error.
  int routing_id = 0;
  int work_id = 0;
  int error_id = 0;
  std::vector<string16> translated_text_chunks;
  message_sender_.GetMessageParameters(0, &routing_id, &work_id, &error_id,
                                       &translated_text_chunks);

  EXPECT_NE(0, error_id);  // Anything but 0 means there was an error.
  EXPECT_TRUE(translated_text_chunks.empty());
}

// Tests that a secure translation is done over a secure connection.
TEST_F(TranslationServiceTest, SecureTranslation) {
  std::vector<string16> text_chunks;
  text_chunks.push_back(ASCIIToUTF16("Hello"));
  translation_service_.Translate(0, 0, 0, text_chunks, "en", "fr",
                                 true /* secure */);

  // Run the task that creates the URLFetcher and sends the request.
  MessageLoop::current()->RunAllPending();

  TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(url_fetcher != NULL);
  EXPECT_TRUE(url_fetcher->original_url().SchemeIsSecure());
}

// Test mixing requests from different renderers.
TEST_F(TranslationServiceTest, MultipleRVRequests) {
  const char* const kExpectedRV1_1 = "Bonjour RV1";
  const char* const kExpectedRV1_2 = "Encore bonjour RV1";
  const char* const kExpectedRV1_3 = "Encore bonjour a nouveau RV1";
  const char* const kExpectedRV2 = "Bonjour RV2";
  const char* const kExpectedRV3 = "Bonjour RV3";

  TextChunks text_chunks;
  text_chunks.push_back(ASCIIToUTF16("Hello RV1"));
  text_chunks.push_back(ASCIIToUTF16("Hello again RV1"));
  translation_service_.Translate(1, 0, 0, text_chunks, "en", "fr", false);
  text_chunks.clear();
  text_chunks.push_back(ASCIIToUTF16("Hello RV2"));
  translation_service_.Translate(2, 0, 0, text_chunks, "en", "fr", false);
  text_chunks.clear();
  text_chunks.push_back(ASCIIToUTF16("Hello again one more time RV1"));
  translation_service_.Translate(1, 0, 1, text_chunks, "en", "fr", false);
  text_chunks.clear();
  text_chunks.push_back(ASCIIToUTF16("Hello RV3"));
  translation_service_.Translate(3, 0, 0, text_chunks, "en", "fr", false);

  // Run the tasks that create the URLFetcher and send the requests.
  MessageLoop::current()->RunAllPending();

  // We should have 3 pending URL fetchers.  (The 2 translate requests for RV1
  // should have been grouped in 1.)  Simluate the translation server responses.
  TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(1);
  ASSERT_TRUE(url_fetcher != NULL);

  TextChunksList text_chunks_list;
  text_chunks.clear();
  text_chunks.push_back(ASCIIToUTF16(kExpectedRV1_1));
  text_chunks.push_back(ASCIIToUTF16(kExpectedRV1_2));
  text_chunks_list.push_back(text_chunks);
  text_chunks.clear();
  text_chunks.push_back(ASCIIToUTF16(kExpectedRV1_3));
  text_chunks_list.push_back(text_chunks);
  SimulateSuccessfulResponse(url_fetcher, text_chunks_list);

  url_fetcher = url_fetcher_factory_.GetFetcherByID(2);
  ASSERT_TRUE(url_fetcher != NULL);
  SimulateSimpleSuccessfulResponse(url_fetcher, &kExpectedRV2, 1);

  url_fetcher = url_fetcher_factory_.GetFetcherByID(3);
  ASSERT_TRUE(url_fetcher != NULL);
  SimulateSimpleSuccessfulResponse(url_fetcher, &kExpectedRV3, 1);

  // This should have triggered 4 ViewMsg_TranslateTextReponse messages.
  ASSERT_EQ(4U, message_sender_.message_count());

  const int kExpectedRoutingID[] = { 1, 1, 2, 3 };
  const int kExpectedWorkID[] = { 0, 1, 0, 0 };
  const size_t kExpectedStringCount[] = { 2U, 1U, 1U, 1U };

  // Test the messages have the expected content.
  for (size_t i = 0; i < 4; i++) {
    SCOPED_TRACE(::testing::Message::Message() << "Iteration " << i);
    int routing_id = 0;
    int work_id = 0;
    int error_id = 0;
    std::vector<string16> translated_text_chunks;
    message_sender_.GetMessageParameters(i, &routing_id, &work_id, &error_id,
                                         &translated_text_chunks);
    EXPECT_EQ(kExpectedRoutingID[i], routing_id);
    EXPECT_EQ(kExpectedWorkID[i], work_id);
    EXPECT_EQ(0, error_id);
    EXPECT_EQ(kExpectedStringCount[i], translated_text_chunks.size());
    // TODO(jcampan): we should compare the strings.
  }
}

// Tests sending more than the max size.
TEST_F(TranslationServiceTest, MoreThanMaxSizeRequests) {
  std::string one_kb_string(1024U, 'A');
  TextChunks text_chunks;
  text_chunks.push_back(ASCIIToUTF16(one_kb_string));
  // Send 2 small requests, than a big one.
  translation_service_.Translate(1, 0, 0, text_chunks, "en", "fr", false);
  translation_service_.Translate(1, 0, 1, text_chunks, "en", "fr", false);
  // We need a string big enough to be more than 30KB on top of the other 2
  // requests, but to be less than 30KB when sent (that sizes includes the
  // other parameters required by the translation server).
  std::string twenty_nine_kb_string(29 * 1024, 'G');
  text_chunks.clear();
  text_chunks.push_back(ASCIIToUTF16(twenty_nine_kb_string));
  translation_service_.Translate(1, 0, 2, text_chunks, "en", "fr", false);

  // Without any task been run, the 2 first requests should have been sent.
  TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(1);
  TextChunks query_strings;
  ExtractQueryStringsFromUploadData(url_fetcher, &query_strings);
  ASSERT_EQ(2U, query_strings.size());
  EXPECT_EQ(one_kb_string, UTF16ToASCII(query_strings[0]));
  EXPECT_EQ(one_kb_string, UTF16ToASCII(query_strings[1]));

  // Then when the task runs, the big request is sent.
  MessageLoop::current()->RunAllPending();

  url_fetcher = url_fetcher_factory_.GetFetcherByID(1);
  query_strings.clear();
  ExtractQueryStringsFromUploadData(url_fetcher, &query_strings);
  ASSERT_EQ(1U, query_strings.size());
  EXPECT_EQ(twenty_nine_kb_string, UTF16ToASCII(query_strings[0]));
}

// Test mixing secure/insecure requests and that secure requests are always sent
// over HTTPS.
TEST_F(TranslationServiceTest, MixedHTTPAndHTTPS) {
  const char* kUnsecureMessage = "Hello";
  const char* kSecureMessage = "Hello_Secure";

  std::vector<string16> text_chunks;
  text_chunks.push_back(ASCIIToUTF16(kUnsecureMessage));
  translation_service_.Translate(0, 0, 0, text_chunks, "en", "fr", false);
  text_chunks.clear();
  text_chunks.push_back(ASCIIToUTF16(kSecureMessage));
  translation_service_.Translate(0, 0, 0, text_chunks, "en", "fr", true);

  // Run the task that creates the URLFetcher and send the request.
  MessageLoop::current()->RunAllPending();

  // We only get the last URLFetcher since we id them based on their routing id
  // which we want to be the same in that test.  We'll just check that as
  // expected the last one is the HTTPS and contains only the secure string.
  TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
  TextChunks query_strings;
  ExtractQueryStringsFromUploadData(url_fetcher, &query_strings);
  ASSERT_EQ(1U, query_strings.size());
  EXPECT_EQ(kSecureMessage, UTF16ToASCII(query_strings[0]));
}
