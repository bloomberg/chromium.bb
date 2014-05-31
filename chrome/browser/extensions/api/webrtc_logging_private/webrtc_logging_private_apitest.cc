// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/webrtc_logging_private/webrtc_logging_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media/webrtc_log_uploader.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

using extensions::Extension;

namespace utils = extension_function_test_utils;

namespace {

static const char kTestLoggingSessionId[] = "0123456789abcdef";
static const char kTestLoggingUrl[] = "dummy url string";

class WebrtcLoggingPrivateApiTest : public ExtensionApiTest {
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest, TestStartStopDiscard) {
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  // Tell the uploader to save the multipart to a buffer instead of uploading.
  std::string multipart;
  g_browser_process->webrtc_log_uploader()->
      OverrideUploadWithBufferForTesting(&multipart);

  // Start

  scoped_refptr<extensions::WebrtcLoggingPrivateStartFunction>
      start_function(new extensions::WebrtcLoggingPrivateStartFunction());
  start_function->set_extension(empty_extension.get());
  start_function->set_has_callback(true);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::ListValue parameters;
  parameters.AppendInteger(extensions::ExtensionTabUtil::GetTabId(contents));
  parameters.AppendString(contents->GetURL().GetOrigin().spec());
  std::string parameter_string;
  base::JSONWriter::Write(&parameters, &parameter_string);

  // TODO(grunell): MaybeRunFunction is suitable for those calls not returning
  // anything.
  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      start_function.get(), parameter_string, browser()));
  ASSERT_FALSE(result.get());

  // Stop

  scoped_refptr<extensions::WebrtcLoggingPrivateStopFunction>
      stop_function(new extensions::WebrtcLoggingPrivateStopFunction());
  stop_function->set_extension(empty_extension.get());
  stop_function->set_has_callback(true);

  result.reset(utils::RunFunctionAndReturnSingleResult(
      stop_function.get(), parameter_string, browser()));
  ASSERT_FALSE(result.get());

  // Discard

  scoped_refptr<extensions::WebrtcLoggingPrivateDiscardFunction>
      discard_function(new extensions::WebrtcLoggingPrivateDiscardFunction());
  discard_function->set_extension(empty_extension.get());
  discard_function->set_has_callback(true);

  result.reset(utils::RunFunctionAndReturnSingleResult(
      discard_function.get(), parameter_string, browser()));
  ASSERT_FALSE(result.get());

  ASSERT_TRUE(multipart.empty());

  g_browser_process->webrtc_log_uploader()->OverrideUploadWithBufferForTesting(
      NULL);
}

// Tests WebRTC diagnostic logging. Sets up the browser to save the multipart
// contents to a buffer instead of uploading it, then verifies it after a calls.
// Example of multipart contents:
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="prod"
//
// Chrome_Linux
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="ver"
//
// 30.0.1554.0
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="guid"
//
// 0
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="type"
//
// webrtc_log
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="app_session_id"
//
// 0123456789abcdef
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="url"
//
// http://127.0.0.1:43213/webrtc/webrtc_jsep01_test.html
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="webrtc_log"; filename="webrtc_log.gz"
// Content-Type: application/gzip
//
// <compressed data (zip)>
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**------
//
IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest, TestStartStopUpload) {
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  // Tell the uploader to save the multipart to a buffer instead of uploading.
  std::string multipart;
  g_browser_process->webrtc_log_uploader()->
      OverrideUploadWithBufferForTesting(&multipart);

  // SetMetaData.

  scoped_refptr<extensions::WebrtcLoggingPrivateSetMetaDataFunction>
      set_meta_data_function(
          new extensions::WebrtcLoggingPrivateSetMetaDataFunction());
  set_meta_data_function->set_extension(empty_extension.get());
  set_meta_data_function->set_has_callback(true);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::ListValue parameters;
  parameters.AppendInteger(extensions::ExtensionTabUtil::GetTabId(contents));
  parameters.AppendString(contents->GetURL().GetOrigin().spec());
  base::DictionaryValue* meta_data_entry = new base::DictionaryValue();
  meta_data_entry->SetString("key", "app_session_id");
  meta_data_entry->SetString("value", kTestLoggingSessionId);
  base::ListValue* meta_data = new base::ListValue();
  meta_data->Append(meta_data_entry);
  meta_data_entry = new base::DictionaryValue();
  meta_data_entry->SetString("key", "url");
  meta_data_entry->SetString("value", kTestLoggingUrl);
  meta_data->Append(meta_data_entry);
  parameters.Append(meta_data);

  std::string parameter_string;
  base::JSONWriter::Write(&parameters, &parameter_string);

  // TODO(grunell): MaybeRunFunction is suitable for those calls not returning
  // anything.
  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      set_meta_data_function.get(), parameter_string, browser()));
  ASSERT_FALSE(result.get());

  // Start.

  scoped_refptr<extensions::WebrtcLoggingPrivateStartFunction>
      start_function(new extensions::WebrtcLoggingPrivateStartFunction());
  start_function->set_extension(empty_extension.get());
  start_function->set_has_callback(true);

  parameters.Clear();
  parameters.AppendInteger(extensions::ExtensionTabUtil::GetTabId(contents));
  parameters.AppendString(contents->GetURL().GetOrigin().spec());
  base::JSONWriter::Write(&parameters, &parameter_string);

  result.reset(utils::RunFunctionAndReturnSingleResult(
      start_function.get(), parameter_string, browser()));
  ASSERT_FALSE(result.get());

  // Stop.

  scoped_refptr<extensions::WebrtcLoggingPrivateStopFunction>
      stop_function(new extensions::WebrtcLoggingPrivateStopFunction());
  stop_function->set_extension(empty_extension.get());
  stop_function->set_has_callback(true);

  result.reset(utils::RunFunctionAndReturnSingleResult(
      stop_function.get(), parameter_string, browser()));
  ASSERT_FALSE(result.get());

  // Upload.

  scoped_refptr<extensions::WebrtcLoggingPrivateUploadFunction>
      upload_function(new extensions::WebrtcLoggingPrivateUploadFunction());
  upload_function->set_extension(empty_extension.get());
  upload_function->set_has_callback(true);

  result.reset(utils::RunFunctionAndReturnSingleResult(
      upload_function.get(), parameter_string, browser()));
  ASSERT_TRUE(result.get());

  ASSERT_FALSE(multipart.empty());

  // Check multipart data.

  const char boundary[] = "------**--yradnuoBgoLtrapitluMklaTelgooG--**----";

  // Remove the compressed data, it may contain "\r\n". Just verify that its
  // size is > 0.
  const char zip_content_type[] = "Content-Type: application/gzip";
  size_t zip_pos = multipart.find(&zip_content_type[0]);
  ASSERT_NE(std::string::npos, zip_pos);
  // Move pos to where the zip begins. - 1 to remove '\0', + 4 for two "\r\n".
  zip_pos += sizeof(zip_content_type) + 3;
  size_t zip_length = multipart.find(boundary, zip_pos);
  ASSERT_NE(std::string::npos, zip_length);
  // Calculate length, adjust for a "\r\n".
  zip_length -= zip_pos + 2;
  ASSERT_GT(zip_length, 0u);
  multipart.erase(zip_pos, zip_length);

  // Check the multipart contents.
  std::vector<std::string> multipart_lines;
  base::SplitStringUsingSubstr(multipart, "\r\n", &multipart_lines);
  ASSERT_EQ(31, static_cast<int>(multipart_lines.size()));

  EXPECT_STREQ(&boundary[0], multipart_lines[0].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"prod\"",
               multipart_lines[1].c_str());
  EXPECT_TRUE(multipart_lines[2].empty());
  EXPECT_NE(std::string::npos, multipart_lines[3].find("Chrome"));

  EXPECT_STREQ(&boundary[0], multipart_lines[4].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"ver\"",
               multipart_lines[5].c_str());
  EXPECT_TRUE(multipart_lines[6].empty());
  // Just check that the version contains a dot.
  EXPECT_NE(std::string::npos, multipart_lines[7].find('.'));

  EXPECT_STREQ(&boundary[0], multipart_lines[8].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"guid\"",
               multipart_lines[9].c_str());
  EXPECT_TRUE(multipart_lines[10].empty());
  EXPECT_STREQ("0", multipart_lines[11].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[12].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"type\"",
               multipart_lines[13].c_str());
  EXPECT_TRUE(multipart_lines[14].empty());
  EXPECT_STREQ("webrtc_log", multipart_lines[15].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[16].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"app_session_id\"",
               multipart_lines[17].c_str());
  EXPECT_TRUE(multipart_lines[18].empty());
  EXPECT_STREQ(kTestLoggingSessionId, multipart_lines[19].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[20].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"url\"",
               multipart_lines[21].c_str());
  EXPECT_TRUE(multipart_lines[22].empty());
  EXPECT_STREQ(kTestLoggingUrl, multipart_lines[23].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[24].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"webrtc_log\";"
               " filename=\"webrtc_log.gz\"",
               multipart_lines[25].c_str());
  EXPECT_STREQ("Content-Type: application/gzip",
               multipart_lines[26].c_str());
  EXPECT_TRUE(multipart_lines[27].empty());
  EXPECT_TRUE(multipart_lines[28].empty());  // The removed zip part.
  std::string final_delimiter = boundary;
  final_delimiter += "--";
  EXPECT_STREQ(final_delimiter.c_str(), multipart_lines[29].c_str());
  EXPECT_TRUE(multipart_lines[30].empty());

  g_browser_process->webrtc_log_uploader()->OverrideUploadWithBufferForTesting(
      NULL);
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest, TestStartStopRtpDump) {
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  // Start RTP dump.
  scoped_refptr<extensions::WebrtcLoggingPrivateStartRtpDumpFunction>
      start_function(
          new extensions::WebrtcLoggingPrivateStartRtpDumpFunction());
  start_function->set_extension(empty_extension.get());
  start_function->set_has_callback(true);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::ListValue parameters;
  parameters.AppendInteger(extensions::ExtensionTabUtil::GetTabId(contents));
  parameters.AppendString(contents->GetURL().GetOrigin().spec());
  parameters.AppendBoolean(true);
  parameters.AppendBoolean(true);
  std::string parameter_string;
  base::JSONWriter::Write(&parameters, &parameter_string);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      start_function.get(), parameter_string, browser()));
  ASSERT_FALSE(result.get());

  // Stop RTP dump.
  scoped_refptr<extensions::WebrtcLoggingPrivateStopRtpDumpFunction>
      stop_function(new extensions::WebrtcLoggingPrivateStopRtpDumpFunction());
  stop_function->set_extension(empty_extension.get());
  stop_function->set_has_callback(true);

  result.reset(utils::RunFunctionAndReturnSingleResult(
      stop_function.get(), parameter_string, browser()));
  ASSERT_FALSE(result.get());
}
