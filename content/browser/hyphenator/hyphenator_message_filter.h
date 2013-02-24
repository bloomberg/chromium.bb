// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HYPHENATOR_HYPHENATOR_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_HYPHENATOR_HYPHENATOR_MESSAGE_FILTER_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {
class RenderProcessHost;

// This class is a message filter that handles a HyphenatorHost message. When
// this class receives a HyphenatorHostMsg_OpenDictionary message, it opens the
// specified dictionary and sends its file handle.
class CONTENT_EXPORT HyphenatorMessageFilter : public BrowserMessageFilter {
 public:
  explicit HyphenatorMessageFilter(RenderProcessHost* render_process_host);

  // Changes the directory that includes dictionary files. This function
  // provides a method that allows applications to change the directory
  // containing hyphenation dictionaries. When a renderer requests a hyphnation
  // dictionary, this class appends a file name (which consists of a locale, a
  // version number, and an extension) and use it as a dictionary file.
  void SetDictionaryBase(const base::FilePath& directory);

  // BrowserMessageFilter implementation.
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  friend class TestHyphenatorMessageFilter;

  virtual ~HyphenatorMessageFilter();

  virtual void OnOpenDictionary(const string16& locale);

  // Opens a hyphenation dictionary for the specified locale. When this locale
  // is an empty string, this function uses US English ("en-US").
  void OpenDictionary(const string16& locale);

  // Sends the hyphenation dictionary file to a renderer in response to its
  // request. If this class cannot open the specified dictionary file, this
  // function sends an IPC::InvalidPlatformFileForTransit value to tell the
  // renderer that a browser cannot open the file.
  void SendDictionary();

  // The RenderProcessHost object that owns this filter. This class uses this
  // object to retrieve the process handle used for creating
  // PlatformFileForTransit objects.
  RenderProcessHost* render_process_host_;

  // The directory that includes dictionary files. The default value is the
  // directory containing the executable file.
  base::FilePath dictionary_base_;

  // A cached dictionary file.
  base::PlatformFile dictionary_file_;

  base::WeakPtrFactory<HyphenatorMessageFilter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HyphenatorMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_HYPHENATOR_HYPHENATOR_MESSAGE_FILTER_H_
