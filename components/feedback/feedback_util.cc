// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_util.h"

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/feedback_uploader.h"
#include "components/feedback/feedback_uploader_factory.h"
#include "components/feedback/proto/common.pb.h"
#include "components/feedback/proto/dom.pb.h"
#include "components/feedback/proto/extension.pb.h"
#include "components/feedback/proto/math.pb.h"
#include "third_party/zlib/google/zip.h"

using feedback::FeedbackData;

namespace {

const char kPngMimeType[] = "image/png";
const char kArbitraryMimeType[] = "application/octet-stream";
const char kHistogramsAttachmentName[] = "histograms.zip";
const char kLogsAttachmentName[] = "system_logs.zip";

#if defined(OS_CHROMEOS)
const int kChromeOSProductId = 208;
#else
const int kChromeBrowserProductId = 237;
#endif

void AddFeedbackData(userfeedback::ExtensionSubmit* feedback_data,
                     const std::string& key, const std::string& value) {
  // Don't bother with empty keys or values
  if (key == "" || value == "") return;
  // Create log_value object and add it to the web_data object
  userfeedback::ProductSpecificData log_value;
  log_value.set_key(key);
  log_value.set_value(value);
  userfeedback::WebData* web_data = feedback_data->mutable_web_data();
  *(web_data->add_product_specific_data()) = log_value;
}

// Adds data as an attachment to feedback_data if the data is non-empty.
void AddAttachment(userfeedback::ExtensionSubmit* feedback_data,
                   const char* name,
                   std::string* data) {
  if (data == NULL || data->empty())
    return;

  userfeedback::ProductSpecificBinaryData* attachment =
      feedback_data->add_product_specific_binary_data();
  attachment->set_mime_type(kArbitraryMimeType);
  attachment->set_name(name);
  attachment->set_data(*data);
}

}  // namespace

namespace feedback_util {

void SendReport(scoped_refptr<FeedbackData> data) {
  if (!data.get()) {
    LOG(ERROR) << "SendReport called with NULL data!";
    NOTREACHED();
    return;
  }

  userfeedback::ExtensionSubmit feedback_data;
  // Unused field, needs to be 0 though.
  feedback_data.set_type_id(0);

  userfeedback::CommonData* common_data = feedback_data.mutable_common_data();
  // We're not using gaia ids, we're using the e-mail field instead.
  common_data->set_gaia_id(0);
  common_data->set_user_email(data->user_email());
  common_data->set_description(data->description());
  common_data->set_source_description_language(data->locale());

  userfeedback::WebData* web_data = feedback_data.mutable_web_data();
  web_data->set_url(data->page_url());
  web_data->mutable_navigator()->set_user_agent(data->user_agent());

  if (data->sys_info()) {
    for (FeedbackData::SystemLogsMap::const_iterator i =
        data->sys_info()->begin(); i != data->sys_info()->end(); ++i) {
      if (FeedbackData::BelowCompressionThreshold(i->second))
        AddFeedbackData(&feedback_data, i->first, i->second);
    }

    AddAttachment(&feedback_data, kLogsAttachmentName, data->compressed_logs());
  }

  if (data->histograms()) {
    AddAttachment(&feedback_data,
                  kHistogramsAttachmentName,
                  data->compressed_histograms());
  }

  if (!data->attached_filename().empty()) {
    // We need to use the UTF8Unsafe methods here to accomodate Windows, which
    // uses wide strings to store filepaths.
    std::string name = base::FilePath::FromUTF8Unsafe(
        data->attached_filename()).BaseName().AsUTF8Unsafe();
    AddAttachment(&feedback_data, name.c_str(), data->attached_filedata());
  }

  // NOTE: Screenshot needs to be processed after system info since we'll get
  // the screenshot dimensions from system info.
  if (data->image() && data->image()->size()) {
    userfeedback::PostedScreenshot screenshot;
    screenshot.set_mime_type(kPngMimeType);

    // Set that we 'have' dimensions of the screenshot. These dimensions are
    // ignored by the server but are a 'required' field in the protobuf.
    userfeedback::Dimensions dimensions;
    dimensions.set_width(0.0);
    dimensions.set_height(0.0);

    *(screenshot.mutable_dimensions()) = dimensions;
    screenshot.set_binary_content(*data->image());

    *(feedback_data.mutable_screenshot()) = screenshot;
  }

  if (data->category_tag().size())
    feedback_data.set_bucket(data->category_tag());

  // Set whether we're reporting from ChromeOS or Chrome on another platform.
  userfeedback::ChromeData chrome_data;
#if defined(OS_CHROMEOS)
  chrome_data.set_chrome_platform(
      userfeedback::ChromeData_ChromePlatform_CHROME_OS);
  userfeedback::ChromeOsData chrome_os_data;
  chrome_os_data.set_category(
      userfeedback::ChromeOsData_ChromeOsCategory_OTHER);
  *(chrome_data.mutable_chrome_os_data()) = chrome_os_data;
  feedback_data.set_product_id(kChromeOSProductId);
#else
  chrome_data.set_chrome_platform(
      userfeedback::ChromeData_ChromePlatform_CHROME_BROWSER);
  userfeedback::ChromeBrowserData chrome_browser_data;
  chrome_browser_data.set_category(
      userfeedback::ChromeBrowserData_ChromeBrowserCategory_OTHER);
  *(chrome_data.mutable_chrome_browser_data()) = chrome_browser_data;
  feedback_data.set_product_id(kChromeBrowserProductId);
#endif

  *(feedback_data.mutable_chrome_data()) = chrome_data;

  // This pointer will eventually get deleted by the PostCleanup class, after
  // we've either managed to successfully upload the report or died trying.
  std::string post_body;
  feedback_data.SerializeToString(&post_body);

  feedback::FeedbackUploader *uploader =
      feedback::FeedbackUploaderFactory::GetForBrowserContext(data->context());
  uploader->QueueReport(post_body);
}

bool ZipString(const base::FilePath& filename,
               const std::string& data, std::string* compressed_logs) {
  base::FilePath temp_path;
  base::FilePath zip_file;

  // Create a temporary directory, put the logs into a file in it. Create
  // another temporary file to receive the zip file in.
  if (!base::CreateNewTempDirectory(base::FilePath::StringType(), &temp_path))
    return false;
  if (base::WriteFile(temp_path.Append(filename), data.c_str(), data.size()) ==
      -1)
    return false;

  bool succeed = base::CreateTemporaryFile(&zip_file) &&
      zip::Zip(temp_path, zip_file, false) &&
      base::ReadFileToString(zip_file, compressed_logs);

  base::DeleteFile(temp_path, true);
  base::DeleteFile(zip_file, false);

  return succeed;
}

}  // namespace feedback_util
