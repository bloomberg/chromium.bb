// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_
#define CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/process.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_platform_file.h"

class FilePath;
class TabContents;

class CONTENT_EXPORT MHTMLGenerationManager
    : public base::RefCountedThreadSafe<
          MHTMLGenerationManager, content::BrowserThread::DeleteOnUIThread> {
 public:
  MHTMLGenerationManager();
  ~MHTMLGenerationManager();

  typedef base::Callback<void(const FilePath& /* path to the MHTML file */,
      int64 /* size of the file */)> GenerateMHTMLCallback;

  // Instructs the render view to generate a MHTML representation of the current
  // page for |tab_contents|.
  void GenerateMHTML(TabContents* tab_contents,
                     const FilePath& file,
                     const GenerateMHTMLCallback& callback);

  // Notification from the renderer that the MHTML generation finished.
  // |mhtml_data_size| contains the size in bytes of the generated MHTML data,
  // or -1 in case of failure.
  void MHTMLGenerated(int job_id, int64 mhtml_data_size);

 private:
  struct Job{
    Job();

    FilePath file_path;

    // The handles to file the MHTML is saved to, for the browser and renderer
    // processes.
    base::PlatformFile browser_file;
    IPC::PlatformFileForTransit renderer_file;

    // The IDs mapping to a specific tab.
    int process_id;
    int routing_id;

    // The callback to call once generation is complete.
    GenerateMHTMLCallback callback;
  };

  // Called on the file thread to create |file|.
  void CreateFile(int job_id,
                  const FilePath& file,
                  base::ProcessHandle renderer_process);

  // Called on the UI thread when the file that should hold the MHTML data has
  // been created.  This returns a handle to that file for the browser process
  // and one for the renderer process. These handles are
  // kInvalidPlatformFileValue if the file could not be opened.
  void FileCreated(int job_id,
                   base::PlatformFile browser_file,
                   IPC::PlatformFileForTransit renderer_file);

  // Called on the file thread to close the file the MHTML was saved to.
  void CloseFile(base::PlatformFile file);

  // Called on the UI thread when a job has been processed (successfully or
  // not).  Closes the file and removes the job from the job map.
  // |mhtml_data_size| is -1 if the MHTML generation failed.
  void JobFinished(int job_id, int64 mhtml_data_size);

  typedef std::map<int, Job> IDToJobMap;
  IDToJobMap id_to_job_;

  DISALLOW_COPY_AND_ASSIGN(MHTMLGenerationManager);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_
