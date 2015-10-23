// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_
#define CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_

#include <map>

#include "base/files/file.h"
#include "base/memory/singleton.h"
#include "base/process/process.h"
#include "ipc/ipc_platform_file.h"

namespace base {
class FilePath;
}

namespace content {

class WebContents;

// The class and all of its members live on the UI thread.  Only static methods
// are executed on other threads.
class MHTMLGenerationManager {
 public:
  static MHTMLGenerationManager* GetInstance();

  // GenerateMHTMLCallback is called to report completion and status of MHTML
  // generation.  On success |file_size| indicates the size of the
  // generated file.  On failure |file_size| is -1.
  typedef base::Callback<void(int64 file_size)> GenerateMHTMLCallback;

  // Instructs the render view to generate a MHTML representation of the current
  // page for |web_contents|.
  void SaveMHTML(WebContents* web_contents,
                 const base::FilePath& file_path,
                 const GenerateMHTMLCallback& callback);

  // Handler for ViewHostMsg_SavedPageAsMHTML (a notification from the renderer
  // that the MHTML generation finished).
  void OnSavedPageAsMHTML(int job_id,
                          bool mhtml_generation_in_renderer_succeeded);

 private:
  friend struct base::DefaultSingletonTraits<MHTMLGenerationManager>;
  class Job;
  enum class JobStatus { SUCCESS, FAILURE };

  MHTMLGenerationManager();
  virtual ~MHTMLGenerationManager();

  // Called on the file thread to create |file|.
  static base::File CreateFile(const base::FilePath& file_path);

  // Called on the UI thread when the file that should hold the MHTML data has
  // been created.
  void OnFileAvailable(int job_id, base::File browser_file);

  // Called on the UI thread when a job has been finished.
  void JobFinished(int job_id, JobStatus job_status);

  // Called on the UI thread after the file got finalized and we have its size.
  void OnFileClosed(int job_id, JobStatus job_status, int64 file_size);

  // Creates and registers a new job.
  int NewJob(WebContents* web_contents, const GenerateMHTMLCallback& callback);

  // Finds job by id.  Returns nullptr if no job with a given id was found.
  Job* FindJob(int job_id);

  // Called when the render process connected to a job exits.
  void RenderProcessExited(Job* job);

  typedef std::map<int, Job*> IDToJobMap;
  IDToJobMap id_to_job_;

  int next_job_id_;

  DISALLOW_COPY_AND_ASSIGN(MHTMLGenerationManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_
