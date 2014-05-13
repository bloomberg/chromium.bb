// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_MEDIA_METADATA_PARSER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_MEDIA_METADATA_PARSER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/api/media_galleries.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"

namespace IPC {
class Message;
}

class Profile;

namespace metadata {

// Parses the media metadata of a Blob safely in a utility process. This class
// expects the MIME type of the Blob to be already determined. It spawns a
// utility process to do further MIME-type specific metadata extraction.
// All public methods and callbacks of this class run on the UI thread.
class SafeMediaMetadataParser : public content::UtilityProcessHostClient {
 public:
  // |metadata_dictionary| is owned by the callback.
  typedef base::Callback<
      void(bool parse_success,
           scoped_ptr<base::DictionaryValue> metadata_dictionary,
           scoped_ptr<std::vector<AttachedImage> > attached_images)>
      DoneCallback;

  SafeMediaMetadataParser(Profile* profile, const std::string& blob_uuid,
                          int64 blob_size, const std::string& mime_type,
                          bool get_attached_images);

  // Should be called on the UI thread. |callback| also runs on the UI thread.
  void Start(const DoneCallback& callback);

 private:
  enum ParserState {
    INITIAL_STATE,
    STARTED_PARSING_STATE,
    FINISHED_PARSING_STATE,
  };

  // Private because content::UtilityProcessHostClient is ref-counted.
  virtual ~SafeMediaMetadataParser();

  // Launches the utility process.  Must run on the IO thread.
  void StartWorkOnIOThread(const DoneCallback& callback);

  // Notification from the utility process when it finishes parsing metadata.
  // Runs on the IO thread.
  void OnParseMediaMetadataFinished(
      bool parse_success, const base::DictionaryValue& metadata_dictionary,
      const std::vector<AttachedImage>& attached_images);

  // Sequence of functions that bounces from the IO thread to the UI thread to
  // read the blob data, then sends the data back to the utility process.
  void OnUtilityProcessRequestBlobBytes(int64 request_id, int64 byte_start,
                                        int64 length);
  void StartBlobReaderOnUIThread(int64 request_id, int64 byte_start,
                                 int64 length);
  void OnBlobReaderDoneOnUIThread(int64 request_id,
                                  scoped_ptr<std::string> data,
                                  int64 /* blob_total_size */);
  void FinishRequestBlobBytes(int64 request_id, scoped_ptr<std::string> data);

  // UtilityProcessHostClient implementation.
  // Runs on the IO thread.
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // All member variables are only accessed on the IO thread.
  Profile* const profile_;
  const std::string blob_uuid_;
  const int64 blob_size_;
  const std::string mime_type_;
  bool get_attached_images_;

  DoneCallback callback_;

  base::WeakPtr<content::UtilityProcessHost> utility_process_host_;

  // Verifies the messages from the utility process came at the right time.
  // Initialized on the UI thread, but only accessed on the IO thread.
  ParserState parser_state_;

  DISALLOW_COPY_AND_ASSIGN(SafeMediaMetadataParser);
};

}  // namespace metadata

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_MEDIA_METADATA_PARSER_H_
