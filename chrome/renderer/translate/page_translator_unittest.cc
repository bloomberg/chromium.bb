// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/renderer/translate/page_translator.h"
#include "chrome/test/render_view_test.h"
#include "net/base/net_errors.h"

class TranslatorTest : public RenderViewTest {
 public:
  TranslatorTest() {}
};

// A TextTranslator used that simply reverse the strings that are provided to
// it.
class ReverseTextTranslator : public TextTranslator {
 public:
  ReverseTextTranslator()
      : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
        work_id_counter_(0) {
  }

  virtual int Translate(const std::vector<string16>& text_chunks,
                        std::string from_lang,
                        std::string to_lang,
                        bool secure,
                        TextTranslator::Delegate* delegate) {
    int work_id = work_id_counter_++;

    std::vector<string16> translated_text_chunks;
    for (std::vector<string16>::const_iterator iter = text_chunks.begin();
         iter != text_chunks.end(); ++iter) {
      translated_text_chunks.push_back(ReverseString(*iter));
    }
    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(
            &ReverseTextTranslator::NotifyDelegate,
            work_id,
            translated_text_chunks,
            delegate));
    return work_id;
  }

 private:
  void NotifyDelegate(int work_id,
                      const std::vector<string16>& text_chunks,
                      TextTranslator::Delegate* delegate) {
    delegate->TextTranslated(work_id, text_chunks);
  }

  string16 ReverseString(const string16& str) {
    string16 result;
    for (string16::const_reverse_iterator iter = str.rbegin();
         iter != str.rend(); ++iter) {
      result.push_back(*iter);
    }
    return result;
  }

  ScopedRunnableMethodFactory<ReverseTextTranslator> method_factory_;

  int work_id_counter_;

  DISALLOW_COPY_AND_ASSIGN(ReverseTextTranslator);
};

// A simple ResourceLoaderBridge that always fails to load.
class DummyResourceLoaderBridge : public webkit_glue::ResourceLoaderBridge {
 public:
  DummyResourceLoaderBridge() { }

  virtual void AppendDataToUpload(const char* data, int data_len) {}
  virtual void AppendFileRangeToUpload(const FilePath& file_path,
                                       uint64 offset, uint64 length) {}
  virtual void SetUploadIdentifier(int64 identifier) {}
  virtual bool Start(Peer* peer) { return false; }
  virtual void Cancel() {}
  virtual void SetDefersLoading(bool value) {}
  virtual void SyncLoad(SyncLoadResponse* response) {
    response->status.set_status(URLRequestStatus::FAILED);
    response->status.set_os_error(net::ERR_FAILED);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyResourceLoaderBridge);
};

// A ChildThread class that creates a dummy resource loader bridge so that
// page with resources can be loaded (as data:...) without asserting.
class TestChildThread : public ChildThread {
 public:
  TestChildThread() {}

  virtual webkit_glue::ResourceLoaderBridge* CreateBridge(
      const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info,
      int host_renderer_id,
      int host_render_view_id) {
    return new DummyResourceLoaderBridge;
  }

  // Overriden so it does not terminate the message loop. That would assert as
  // the message loop is running in the test.
  virtual void OnProcessFinalRelease() { }
};

// Tests that we parse and change text in pages correctly.
// It loads all the <name>_ORIGINAL.html files under the
// chrome/test/data/translate directory.  There must be a matching
// <name>_TRANLSATED.html in the directory.
// The _ORIGINAL page is loaded and translated.
// The result is compared to the _TRANSLATED page.
// Note: _TRANSLATED.html files can be generated using the reverse_text.py
// script located in the chrome/test/data/translate directory.
// TODO(jcampan): http://crbug.com/32217 This test is disabled as it sometimes
//                fails on Windows and always fails on Unix.  We need to improve
//                RenderViewTest so it supports loading tags that contain links
//                and sub-resources.
TEST_F(TranslatorTest, DISABLED_TranslatePages) {
  // Create the RenderThread singleton. Rendering pages requires a
  // VisitedLinkSlave that this object creates.
  RenderThread render_thread;

  // Create the ChildThread singleton. It is used to create the resource
  // bridges. (It is owned by the ChildProcess.)
  ChildProcess::current()->set_main_thread(new TestChildThread());

  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_dir));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("chrome"));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("test"));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("data"));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("translate"));

  file_util::FileEnumerator file_enumerator(
      data_dir, false, file_util::FileEnumerator::FILES,
      FILE_PATH_LITERAL("*_ORIGINAL.html"));

  FilePath original_file_path = file_enumerator.Next();
  while (!original_file_path.empty()) {
    // Locate the _TRANSLATED.html file.
    FilePath::StringType original_base = original_file_path.BaseName().value();
    LOG(INFO) << "Processing file " << original_base;
    size_t orig_index =
        original_base.rfind(FILE_PATH_LITERAL("_ORIGINAL.html"));
    ASSERT_NE(FilePath::StringType::npos, orig_index);
    FilePath::StringType translated_base = original_base.substr(0, orig_index) +
        FILE_PATH_LITERAL("_TRANSLATED.html");

    FilePath translated_file_path(original_file_path.DirName());
    translated_file_path = translated_file_path.Append(translated_base);

    ASSERT_TRUE(file_util::PathExists(translated_file_path));

    // Load the original file.
    int64 size;
    ASSERT_TRUE(file_util::GetFileSize(original_file_path, &size));
    scoped_array<char> buffer(new char[static_cast<size_t>(size) + 1]);
    ASSERT_EQ(size, file_util::ReadFile(original_file_path, buffer.get(),
                                        static_cast<int>(size)));
    buffer[static_cast<size_t>(size)] = '\0';
    LoadHTML(buffer.get());

    WebKit::WebFrame* web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame);

    // Translate it.
    ReverseTextTranslator text_translator;
    PageTranslator translator(&text_translator);
    translator.Translate(web_frame, "en", "fr");

    // Translation is asynchronous, so we need to process the pending messages
    // to make it happen.
    MessageLoop::current()->RunAllPending();

    WebKit::WebString actual_translated_contents = web_frame->contentAsMarkup();

    // Load the translated page.
    ASSERT_TRUE(file_util::GetFileSize(translated_file_path, &size));
    buffer.reset(new char[static_cast<size_t>(size) + 1]);
    ASSERT_EQ(size, file_util::ReadFile(translated_file_path, buffer.get(),
                                        static_cast<int>(size)));
    buffer[static_cast<size_t>(size)] = '\0';
    LoadHTML(buffer.get());

    web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame);
    WebKit::WebString expected_translated_contents =
        web_frame->contentAsMarkup();

    EXPECT_EQ(expected_translated_contents.length(),
              actual_translated_contents.length());

    // We compare the actual and expected results by chunks of 80 chars to make
    // debugging easier.
    int max = std::min(expected_translated_contents.length(),
                       actual_translated_contents.length());
    int index = 0;
    while (index < max) {
      int len = std::min(80, max - index);
      string16 expected(expected_translated_contents.data() + index, len);
      string16 actual(actual_translated_contents.data() + index, len);
      ASSERT_EQ(expected, actual);
      index += 80;
    }

    // Iterate to the next file to test.
    original_file_path = file_enumerator.Next();
  }
}
