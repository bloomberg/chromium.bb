// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_file_browser_private_api.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/html_dialog_view.h"
#include "chrome/browser/ui/webui/extension_icon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/file_browser_handler.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/local_file_system_file_util.h"
#include "ui/base/l10n/l10n_util.h"

// Error messages.
const char kFileError[] = "File error %d";
const char kInvalidFileUrl[] = "Invalid file URL";

// Internal task ids.
const char kEnqueueTaskId[] = "enqueue";

const int kReadOnlyFilePermissions = base::PLATFORM_FILE_OPEN |
                                     base::PLATFORM_FILE_READ |
                                     base::PLATFORM_FILE_EXCLUSIVE_READ |
                                     base::PLATFORM_FILE_ASYNC;

const int kReadWriteFilePermissions = base::PLATFORM_FILE_OPEN |
                                      base::PLATFORM_FILE_CREATE |
                                      base::PLATFORM_FILE_OPEN_ALWAYS |
                                      base::PLATFORM_FILE_CREATE_ALWAYS |
                                      base::PLATFORM_FILE_READ |
                                      base::PLATFORM_FILE_WRITE |
                                      base::PLATFORM_FILE_EXCLUSIVE_READ |
                                      base::PLATFORM_FILE_EXCLUSIVE_WRITE |
                                      base::PLATFORM_FILE_ASYNC |
                                      base::PLATFORM_FILE_TRUNCATE |
                                      base::PLATFORM_FILE_WRITE_ATTRIBUTES;

typedef std::pair<int, const FileBrowserHandler* > LastUsedHandler;
typedef std::vector<LastUsedHandler> LastUsedHandlerList;

typedef std::vector<const FileBrowserHandler*> ActionList;


// Breaks down task_id that is used between getFileTasks() and executeTask() on
// its building blocks. task_id field the following structure:
//     <extension-id>|<task-action-id>
// Currently, the only supported task-type is of 'context'.
bool CrackTaskIdentifier(const std::string& task_id,
                         std::string* target_extension_id,
                         std::string* action_id) {
  std::vector<std::string> result;
  int count = Tokenize(task_id, std::string("|"), &result);
  if (count != 2)
    return false;
  *target_extension_id = result[0];
  *action_id = result[1];
  return true;
}

std::string MakeTaskID(const char* extension_id,
                       const char*  action_id) {
  return base::StringPrintf("%s|%s", extension_id, action_id);
}

bool GetFileBrowserHandlers(Profile* profile,
                           const GURL& selected_file_url,
                           ActionList* results) {
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return false;  // In unit-tests, we may not have an ExtensionService.

  for (ExtensionList::const_iterator iter = service->extensions()->begin();
       iter != service->extensions()->end();
       ++iter) {
    const Extension* extension = iter->get();
    if (!extension->file_browser_handlers())
      continue;

    for (Extension::FileBrowserHandlerList::const_iterator action_iter =
             extension->file_browser_handlers()->begin();
        action_iter != extension->file_browser_handlers()->end();
        ++action_iter) {
      const FileBrowserHandler* action = action_iter->get();
      if (!action->MatchesURL(selected_file_url))
        continue;

      results->push_back(action_iter->get());
    }
  }
  return true;
}

bool SortByLastUsedTimestampDesc(const LastUsedHandler& a,
                                 const LastUsedHandler& b) {
  return a.first > b.first;
}

// TODO(zelidrag): Wire this with ICU to make this sort I18N happy.
bool SortByTaskName(const LastUsedHandler& a, const LastUsedHandler& b) {
  return base::strcasecmp(a.second->title().data(),
                          b.second->title().data()) > 0;
}

// Given the list of selected files, returns array of context menu tasks
// that are shared
bool FindCommonTasks(Profile* profile,
                     ListValue* files_list,
                     LastUsedHandlerList* named_action_list) {
  named_action_list->clear();
  ActionList common_tasks;
  for (size_t i = 0; i < files_list->GetSize(); ++i) {
    std::string file_url;
    if (!files_list->GetString(i, &file_url))
      return false;

    ActionList file_actions;
    if (!GetFileBrowserHandlers(profile, GURL(file_url), &file_actions))
      return false;
    // If there is nothing to do for one file, the intersection of tasks for all
    // files will be empty at the end.
    if (!file_actions.size()) {
      common_tasks.clear();
      return true;
    }
    // For the very first file, just copy elements.
    if (i == 0) {
      common_tasks.insert(common_tasks.begin(),
                          file_actions.begin(),
                          file_actions.end());
      std::sort(common_tasks.begin(), common_tasks.end());
    } else if (common_tasks.size()) {
      // For all additional files, find intersection between the accumulated
      // and file specific set.
      std::sort(file_actions.begin(), file_actions.end());
      ActionList intersection(common_tasks.size());
      ActionList::iterator intersection_end =
          std::set_intersection(common_tasks.begin(),
                                common_tasks.end(),
                                file_actions.begin(),
                                file_actions.end(),
                                intersection.begin());
      common_tasks.clear();
      common_tasks.insert(common_tasks.begin(),
                          intersection.begin(),
                          intersection_end);
      std::sort(common_tasks.begin(), common_tasks.end());
    }
  }

  const DictionaryValue* prefs_tasks =
      profile->GetPrefs()->GetDictionary(prefs::kLastUsedFileBrowserHandlers);
  for (ActionList::const_iterator iter = common_tasks.begin();
       iter != common_tasks.end(); ++iter) {
    // Get timestamp of when this task was used last time.
    int last_used_timestamp = 0;
    prefs_tasks->GetInteger(MakeTaskID((*iter)->extension_id().data(),
                                       (*iter)->id().data()),
                             &last_used_timestamp);
    named_action_list->push_back(LastUsedHandler(last_used_timestamp, *iter));
  }
  // Sort by the last used descending.
  std::sort(named_action_list->begin(), named_action_list->end(),
            SortByLastUsedTimestampDesc);
  if (named_action_list->size() > 1) {
    // Sort the rest by name.
    std::sort(named_action_list->begin() + 1, named_action_list->end(),
              SortByTaskName);
  }
  return true;
}

// Update file handler usage stats.
void UpdateFileHandlerUsageStats(Profile* profile, const std::string& task_id) {
  if (!profile || !profile->GetPrefs())
    return;
  DictionaryPrefUpdate prefs_usage_update(profile->GetPrefs(),
      prefs::kLastUsedFileBrowserHandlers);
  prefs_usage_update->SetWithoutPathExpansion(task_id,
      new FundamentalValue(
          static_cast<int>(base::Time::Now().ToInternalValue()/
                           base::Time::kMicrosecondsPerSecond)));
}


class LocalFileSystemCallbackDispatcher
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  explicit LocalFileSystemCallbackDispatcher(
      RequestLocalFileSystemFunction* function,
      Profile* profile,
      int child_id,
      scoped_refptr<const Extension> extension)
      : function_(function),
        profile_(profile),
        child_id_(child_id),
        extension_(extension)  {
    DCHECK(function_);
  }

  // fileapi::FileSystemCallbackDispatcher overrides.
  virtual void DidSucceed() OVERRIDE {
    NOTREACHED();
  }

  virtual void DidGetLocalPath(const FilePath& local_path) {
    NOTREACHED();
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& info,
                               const FilePath& unused) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidWrite(int64 bytes, bool complete) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidOpenFileSystem(const std::string& name,
                                 const GURL& root_path) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    // Set up file permission access.
    if (!SetupFileSystemAccessPermissions()) {
      DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
      return;
    }

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(function_,
            &RequestLocalFileSystemFunction::RespondSuccessOnUIThread,
            name,
            root_path));
  }

  virtual void DidFail(base::PlatformFileError error_code) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(function_,
            &RequestLocalFileSystemFunction::RespondFailedOnUIThread,
            error_code));
  }

 private:

  // Grants file system access permissions to file browser component.
  bool SetupFileSystemAccessPermissions() {
    if (!extension_.get())
      return false;

    // Make sure that only component extension can access the entire
    // local file system.
    if (extension_->location() != Extension::COMPONENT
#ifndef NDEBUG
      && !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposePrivateExtensionApi)
#endif
        ) {
      NOTREACHED() << "Private method access by non-component extension "
                   << extension_->id();
      return false;
    }

    fileapi::FileSystemPathManager* path_manager =
        profile_->GetFileSystemContext()->path_manager();
    fileapi::ExternalFileSystemMountPointProvider* provider =
        path_manager->external_provider();
    if (!provider)
      return false;

    // Grant full access to File API from this component extension.
    provider->GrantFullAccessToExtension(extension_->id());

    // Grant R/W file permissions to the renderer hosting component
    // extension for all paths exposed by our local file system provider.
    std::vector<FilePath> root_dirs = provider->GetRootDirectories();
    for (std::vector<FilePath>::iterator iter = root_dirs.begin();
         iter != root_dirs.end();
         ++iter) {
      ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
          child_id_, *iter, kReadWriteFilePermissions);
    }
    return true;
  }

  RequestLocalFileSystemFunction* function_;
  Profile* profile_;
  // Renderer process id.
  int child_id_;
  // Extension source URL.
  scoped_refptr<const Extension> extension_;
  DISALLOW_COPY_AND_ASSIGN(LocalFileSystemCallbackDispatcher);
};

void RequestLocalFileSystemFunction::RequestOnFileThread(
    const GURL& source_url, int child_id) {
  fileapi::FileSystemOperation* operation =
      new fileapi::FileSystemOperation(
          new LocalFileSystemCallbackDispatcher(
              this,
              profile(),
              child_id,
              GetExtension()),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          profile()->GetFileSystemContext(),
          NULL);
  GURL origin_url = source_url.GetOrigin();
  operation->OpenFileSystem(origin_url, fileapi::kFileSystemTypeExternal,
                            false);     // create
}

bool RequestLocalFileSystemFunction::RunImpl() {
  if (!dispatcher() || !dispatcher()->render_view_host() ||
      !dispatcher()->render_view_host()->process())
    return false;

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &RequestLocalFileSystemFunction::RequestOnFileThread,
          source_url_,
          dispatcher()->render_view_host()->process()->id()));
  // Will finish asynchronously.
  return true;
}

void RequestLocalFileSystemFunction::RespondSuccessOnUIThread(
    const std::string& name, const GURL& root_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  result_.reset(new DictionaryValue());
  DictionaryValue* dict = reinterpret_cast<DictionaryValue*>(result_.get());
  dict->SetString("name", name);
  dict->SetString("path", root_path.spec());
  dict->SetInteger("error", base::PLATFORM_FILE_OK);
  SendResponse(true);
}

void RequestLocalFileSystemFunction::RespondFailedOnUIThread(
    base::PlatformFileError error_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  error_ = base::StringPrintf(kFileError, static_cast<int>(error_code));
  SendResponse(false);
}

bool GetFileTasksFileBrowserFunction::RunImpl() {
  ListValue* files_list = NULL;
  if (!args_->GetList(0, &files_list))
    return false;

  ListValue* result_list = new ListValue();
  result_.reset(result_list);

  LastUsedHandlerList common_tasks;
  if (!FindCommonTasks(profile_, files_list, &common_tasks))
    return false;

  ExtensionService* service = profile_->GetExtensionService();
  for (LastUsedHandlerList::const_iterator iter = common_tasks.begin();
       iter != common_tasks.end();
       ++iter) {
    const std::string extension_id = iter->second->extension_id();
    const Extension* extension = service->GetExtensionById(extension_id, false);
    CHECK(extension);
    DictionaryValue* task = new DictionaryValue();
    task->SetString("taskId", MakeTaskID(extension_id.data(),
                                         iter->second->id().data()));
    task->SetString("title", iter->second->title());
    // TODO(zelidrag): Figure out how to expose icon URL that task defined in
    // manifest instead of the default extension icon.
    GURL icon =
        ExtensionIconSource::GetIconURL(extension,
                                        Extension::EXTENSION_ICON_BITTY,
                                        ExtensionIconSet::MATCH_BIGGER,
                                        false);     // grayscale
    task->SetString("iconUrl", icon.spec());
    result_list->Append(task);
  }

  // TODO(zelidrag, serya): Add intent content tasks to result_list once we
  // implement that API.
  SendResponse(true);
  return true;
}

class ExecuteTasksFileSystemCallbackDispatcher
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  explicit ExecuteTasksFileSystemCallbackDispatcher(
      ExecuteTasksFileBrowserFunction* function,
      Profile* profile,
      int child_id,
      const GURL& source_url,
      scoped_refptr<const Extension> extension,
      const std::string task_id,
      const std::vector<GURL>& file_urls)
      : function_(function),
        profile_(profile),
        source_url_(source_url),
        extension_(extension),
        task_id_(task_id),
        origin_file_urls_(file_urls) {
    DCHECK(function_);
  }

  // fileapi::FileSystemCallbackDispatcher overrides.
  virtual void DidSucceed() OVERRIDE {
    NOTREACHED();
  }

  virtual void DidGetLocalPath(const FilePath& local_path) {
    NOTREACHED();
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& info,
                               const FilePath& unused) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidWrite(int64 bytes, bool complete) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidOpenFileSystem(const std::string& file_system_name,
                                 const GURL& file_system_root) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    ExecuteTasksFileBrowserFunction::FileDefinitionList file_list;
    for (std::vector<GURL>::iterator iter = origin_file_urls_.begin();
         iter != origin_file_urls_.end();
         ++iter) {
      // Set up file permission access.
      ExecuteTasksFileBrowserFunction::FileDefinition file;
      if (!SetupFileAccessPermissions(*iter, &file.target_file_url,
                                      &file.virtual_path, &file.is_directory)) {
        continue;
      }
      file_list.push_back(file);
    }
    if (file_list.empty()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(function_,
              &ExecuteTasksFileBrowserFunction::ExecuteFailedOnUIThread));
      return;
    }

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(function_,
            &ExecuteTasksFileBrowserFunction::ExecuteFileActionsOnUIThread,
            task_id_,
            file_system_name,
            file_system_root,
            file_list));
  }

  virtual void DidFail(base::PlatformFileError error_code) OVERRIDE {
    LOG(WARNING) << "Local file system cant be resolved";
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(function_,
            &ExecuteTasksFileBrowserFunction::ExecuteFailedOnUIThread));
  }

 private:
  // Checks legitimacy of file url and grants file RO access permissions from
  // handler (target) extension and its renderer process.
  bool SetupFileAccessPermissions(const GURL& origin_file_url,
     GURL* target_file_url, FilePath* file_path, bool* is_directory) {

    if (!extension_.get())
      return false;

    GURL file_origin_url;
    FilePath virtual_path;
    fileapi::FileSystemType type;
    if (!CrackFileSystemURL(origin_file_url, &file_origin_url, &type,
                            &virtual_path)) {
      return false;
    }

    if (type != fileapi::kFileSystemTypeExternal)
      return false;

    fileapi::FileSystemPathManager* path_manager =
        profile_->GetFileSystemContext()->path_manager();
    if (!path_manager->IsAccessAllowed(file_origin_url,
                                       type,
                                       virtual_path)) {
      return false;
    }

    // Make sure this url really being used by the right caller extension.
    if (source_url_.GetOrigin() != file_origin_url) {
      DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
      return false;
    }

    FilePath root_path =
        path_manager->ValidateFileSystemRootAndGetPathOnFileThread(
          file_origin_url,
          fileapi::kFileSystemTypeExternal,
          virtual_path,
          false);     // create
    FilePath final_file_path = root_path.Append(virtual_path);

    // Check if this file system entry exists first.
    base::PlatformFileInfo file_info;
    FilePath platform_path;
    fileapi::FileSystemOperationContext file_system_operation_context(
        profile_->GetFileSystemContext(),
        fileapi::LocalFileSystemFileUtil::GetInstance());
    if (base::PLATFORM_FILE_OK !=
            fileapi::FileSystemFileUtil::GetInstance()->GetFileInfo(
                &file_system_operation_context, final_file_path, &file_info,
                &platform_path)) {
      return false;
    }

    fileapi::ExternalFileSystemMountPointProvider* external_provider =
        path_manager->external_provider();
    if (!external_provider)
      return false;

    // TODO(zelidrag): Let's just prevent all symlinks for now. We don't want a
    // USB drive content to point to something in the rest of the file system.
    // Ideally, we should permit symlinks within the boundary of the same
    // virtual mount point.
    if (file_info.is_symbolic_link)
      return false;

    // Get task details.
    std::string target_extension_id;
    std::string action_id;
    if (!CrackTaskIdentifier(task_id_, &target_extension_id,
                             &action_id)) {
      return false;
    }

    // TODO(zelidrag): Add explicit R/W + R/O permissions for non-component
    // extensions.

    // Get target extension's process.
    RenderProcessHost* target_host =
        profile_->GetExtensionProcessManager()->GetExtensionProcess(
            target_extension_id);
    if (!target_host)
      return false;

    // Grant R/O access permission to non-component extension and R/W to
    // component extensions.
    ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
        target_host->id(), final_file_path,
        extension_->location() != Extension::COMPONENT ?
            kReadOnlyFilePermissions : kReadWriteFilePermissions);

    // Grant access to this particular file to target extension. This will
    // ensure that the target extension can access only this FS entry and
    // prevent from traversing FS hierarchy upward.
    external_provider->GrantFileAccessToExtension(target_extension_id,
                                                  virtual_path);

    // Output values.
    GURL target_origin_url(Extension::GetBaseURLFromExtensionId(
        target_extension_id));
    GURL base_url = fileapi::GetFileSystemRootURI(target_origin_url,
        fileapi::kFileSystemTypeExternal);
    *target_file_url = GURL(base_url.spec() + virtual_path.value());
    FilePath root(FILE_PATH_LITERAL("/"));
    *file_path = root.Append(virtual_path);
    *is_directory = file_info.is_directory;
    return true;
  }

  ExecuteTasksFileBrowserFunction* function_;
  Profile* profile_;
  // Extension source URL.
  GURL source_url_;
  scoped_refptr<const Extension> extension_;
  std::string task_id_;
  std::vector<GURL> origin_file_urls_;
  DISALLOW_COPY_AND_ASSIGN(ExecuteTasksFileSystemCallbackDispatcher);
};

bool ExecuteTasksFileBrowserFunction::RunImpl() {
  // First param is task id that was to the extension with getFileTasks call.
  std::string task_id;
  if (!args_->GetString(0, &task_id) || !task_id.size())
    return false;

  // The second param is the list of files that need to be executed with this
  // task.
  ListValue* files_list = NULL;
  if (!args_->GetList(1, &files_list))
    return false;

  if (!files_list->GetSize())
    return true;

  return InitiateFileTaskExecution(task_id, files_list);
}

bool ExecuteTasksFileBrowserFunction::InitiateFileTaskExecution(
    const std::string& task_id, ListValue* files_list) {
  std::vector<GURL> file_urls;
  for (size_t i = 0; i < files_list->GetSize(); i++) {
    std::string origin_file_url;
    if (!files_list->GetString(i, &origin_file_url)) {
      error_ = kInvalidFileUrl;
      return false;
    }
    file_urls.push_back(GURL(origin_file_url));
  }
  // Get local file system instance on file thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &ExecuteTasksFileBrowserFunction::RequestFileEntryOnFileThread,
          source_url_,
          task_id,
          file_urls));
  result_.reset(new FundamentalValue(true));
  return true;
}

void ExecuteTasksFileBrowserFunction::RequestFileEntryOnFileThread(
    const GURL& source_url, const std::string& task_id,
    const std::vector<GURL>& file_urls) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  fileapi::FileSystemOperation* operation =
      new fileapi::FileSystemOperation(
          new ExecuteTasksFileSystemCallbackDispatcher(
              this,
              profile(),
              dispatcher()->render_view_host()->process()->id(),
              source_url,
              GetExtension(),
              task_id,
              file_urls),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          profile()->GetFileSystemContext(),
          NULL);
  GURL origin_url = source_url.GetOrigin();
  operation->OpenFileSystem(origin_url, fileapi::kFileSystemTypeExternal,
                            false);     // create
}

void ExecuteTasksFileBrowserFunction::ExecuteFailedOnUIThread() {
  SendResponse(false);
}


void ExecuteTasksFileBrowserFunction::ExecuteFileActionsOnUIThread(
    const std::string& task_id,
    const std::string& file_system_name,
    const GURL& file_system_root,
    const FileDefinitionList& file_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return;
  // Get task details.
  std::string handler_extension_id;
  std::string action_id;
  if (!CrackTaskIdentifier(task_id, &handler_extension_id,
                           &action_id)) {
    LOG(WARNING) << "Invalid task " << task_id;
    SendResponse(false);
    return;
  }

  const Extension* extension = service->GetExtensionById(handler_extension_id,
                                                         false);
  if (!extension) {
    SendResponse(false);
    return;
  }

  ExtensionEventRouter* event_router = profile_->GetExtensionEventRouter();
  if (!event_router) {
    SendResponse(false);
    return;
  }

  scoped_ptr<ListValue> event_args(new ListValue());
  event_args->Append(Value::CreateStringValue(action_id));
  DictionaryValue* details = new DictionaryValue();
  event_args->Append(details);
  // Get file definitions. These will be replaced with Entry instances by
  // chromeHidden.Event.dispatchJSON() method from even_binding.js.
  ListValue* files_urls = new ListValue();
  details->Set("entries", files_urls);
  for (FileDefinitionList::const_iterator iter = file_list.begin();
       iter != file_list.end();
       ++iter) {
    DictionaryValue* file_def = new DictionaryValue();
    files_urls->Append(file_def);
    file_def->SetString("fileSystemName", file_system_name);
    file_def->SetString("fileSystemRoot", file_system_root.spec());
    file_def->SetString("fileFullPath", iter->virtual_path.value());
    file_def->SetBoolean("fileIsDirectory", iter->is_directory);
  }
  // Get tab id.
  Browser* browser = GetCurrentBrowser();
  if (browser) {
    TabContents* contents = browser->GetSelectedTabContents();
    if (contents)
      details->SetInteger("tab_id", ExtensionTabUtil::GetTabId(contents));
  }

  UpdateFileHandlerUsageStats(profile_, task_id);

  std::string json_args;
  base::JSONWriter::Write(event_args.get(), false, &json_args);
  event_router->DispatchEventToExtension(
      handler_extension_id, std::string("fileBrowserHandler.onExecute"),
      json_args, profile_,
      GURL());
  SendResponse(true);
}

FileDialogFunction::FileDialogFunction() {
}

FileDialogFunction::~FileDialogFunction() {
}

// static
FileDialogFunction::Callback
FileDialogFunction::Callback::null_(NULL, NULL, NULL);

// static
FileDialogFunction::Callback::Map FileDialogFunction::Callback::map_;

// static
void FileDialogFunction::Callback::Add(int32 tab_id,
                                     SelectFileDialog::Listener* listener,
                                     HtmlDialogView* dialog,
                                     void* params) {
  if (map_.find(tab_id) == map_.end()) {
    map_.insert(std::make_pair(tab_id, Callback(listener, dialog, params)));
  } else {
    DLOG_ASSERT("FileDialogFunction::AddCallback tab_id already present");
  }
}

// static
void FileDialogFunction::Callback::Remove(int32 tab_id) {
  map_.erase(tab_id);
}

// static
const FileDialogFunction::Callback&
FileDialogFunction::Callback::Find(int32 tab_id) {
  Callback::Map::const_iterator it = map_.find(tab_id);
  return (it == map_.end()) ? null_ : it->second;
}


int32 FileDialogFunction::GetTabId() const {
  return dispatcher()->delegate()->associated_tab_contents()->
    controller().session_id().id();
}

const FileDialogFunction::Callback& FileDialogFunction::GetCallback() const {
  if (!dispatcher() || !dispatcher()->delegate() ||
      !dispatcher()->delegate()->associated_tab_contents()) {
    return Callback::null();
  }
  return Callback::Find(GetTabId());
}

void FileDialogFunction::CloseDialog(HtmlDialogView* dialog) {
  DCHECK(dialog);
  TabContents* contents = dispatcher()->delegate()->associated_tab_contents();
  if (contents)
    dialog->CloseContents(contents);
}

// GetFileSystemRootPathOnFileThread can only be called from the file thread,
// so here we are. This function takes a vector of virtual paths, converts
// them to local paths and calls GetLocalPathsResponseOnUIThread with the
// result vector, on the UI thread.
void FileDialogFunction::GetLocalPathsOnFileThread(const UrlList& file_urls,
                                                   const std::string& task_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePathList selected_files;

  // FilePath(virtual_path) doesn't work on win, so limit this to ChromeOS.
#if defined(OS_CHROMEOS)
  GURL origin_url = source_url().GetOrigin();
  fileapi::FileSystemPathManager* path_manager =
      profile()->GetFileSystemContext()->path_manager();

  size_t len = file_urls.size();
  selected_files.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    const GURL& file_url = file_urls[i];
    GURL file_origin_url;
    FilePath virtual_path;
    fileapi::FileSystemType type;
    if (!CrackFileSystemURL(file_url, &file_origin_url, &type,
                            &virtual_path)) {
      continue;
    }
    if (type != fileapi::kFileSystemTypeExternal) {
      NOTREACHED();
      continue;
    }
    FilePath root = path_manager->ValidateFileSystemRootAndGetPathOnFileThread(
        origin_url,
        fileapi::kFileSystemTypeExternal,
        FilePath(virtual_path),
        false);
    if (!root.empty()) {
      selected_files.push_back(root.Append(virtual_path));
    } else {
      LOG(WARNING) << "GetLocalPathsOnFileThread failed "
                   << file_url.spec();
    }
  }
#endif

  if (!selected_files.empty()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
            &FileDialogFunction::GetLocalPathsResponseOnUIThread,
            selected_files, task_id));
  }
}

bool SelectFileFunction::RunImpl() {
  if (args_->GetSize() != 2) {
    return false;
  }
  std::string file_url;
  args_->GetString(0, &file_url);
  UrlList file_paths;
  file_paths.push_back(GURL(file_url));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &SelectFileFunction::GetLocalPathsOnFileThread,
          file_paths, std::string()));

  return true;
}

void SelectFileFunction::GetLocalPathsResponseOnUIThread(
    const FilePathList& files, const std::string& task_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (files.size() != 1) {
    SendResponse(false);
    return;
  }
  int index;
  args_->GetInteger(1, &index);
  const Callback& callback = GetCallback();
  DCHECK(!callback.IsNull());
  if (!callback.IsNull()) {
    // Must do this before callback, as the callback may delete listeners
    // waiting for window close.
    CloseDialog(callback.dialog());
    callback.listener()->FileSelected(files[0],
                                      index,
                                      callback.params());
  }
  SendResponse(true);
}


ViewFilesFunction::ViewFilesFunction() {
}

ViewFilesFunction::~ViewFilesFunction() {
}

bool ViewFilesFunction::RunImpl() {
  if (args_->GetSize() < 1) {
    return false;
  }

  ListValue* path_list = NULL;
  args_->GetList(0, &path_list);
  DCHECK(path_list);

  std::string internal_task_id;
  args_->GetString(1, &internal_task_id);

  std::string virtual_path;
  size_t len = path_list->GetSize();
  UrlList file_urls;
  file_urls.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    path_list->GetString(i, &virtual_path);
    file_urls.push_back(GURL(virtual_path));
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &ViewFilesFunction::GetLocalPathsOnFileThread,
          file_urls, internal_task_id));

  return true;
}

void ViewFilesFunction::GetLocalPathsResponseOnUIThread(
    const FilePathList& files, const std::string& internal_task_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (FilePathList::const_iterator iter = files.begin();
       iter != files.end();
       ++iter) {
    FileManagerUtil::ViewItem(*iter, internal_task_id == kEnqueueTaskId);
  }
  UpdateFileHandlerUsageStats(profile_, internal_task_id);
  SendResponse(true);
}

SelectFilesFunction::SelectFilesFunction() {
}

SelectFilesFunction::~SelectFilesFunction() {
}

bool SelectFilesFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    return false;
  }

  ListValue* path_list = NULL;
  args_->GetList(0, &path_list);
  DCHECK(path_list);

  std::string virtual_path;
  size_t len = path_list->GetSize();
  UrlList file_urls;
  file_urls.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    path_list->GetString(i, &virtual_path);
    file_urls.push_back(GURL(virtual_path));
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &SelectFilesFunction::GetLocalPathsOnFileThread,
          file_urls, std::string()));

  return true;
}

void SelectFilesFunction::GetLocalPathsResponseOnUIThread(
    const FilePathList& files, const std::string& internal_task_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const Callback& callback = GetCallback();
  DCHECK(!callback.IsNull());
  if (!callback.IsNull()) {
    // Must do this before callback, as the callback may delete listeners
    // waiting for window close.
    CloseDialog(callback.dialog());
    callback.listener()->MultiFilesSelected(files, callback.params());
  }
  SendResponse(true);
}

bool CancelFileDialogFunction::RunImpl() {
  const Callback& callback = GetCallback();
  DCHECK(!callback.IsNull());
  if (!callback.IsNull()) {
    // Must do this before callback, as the callback may delete listeners
    // waiting for window close.
    CloseDialog(callback.dialog());
    callback.listener()->FileSelectionCanceled(callback.params());
  }
  SendResponse(true);
  return true;
}

bool FileDialogStringsFunction::RunImpl() {
  result_.reset(new DictionaryValue());
  DictionaryValue* dict = reinterpret_cast<DictionaryValue*>(result_.get());

#define SET_STRING(ns, id) \
  dict->SetString(#id, l10n_util::GetStringUTF16(ns##_##id))

  SET_STRING(IDS, LOCALE_FMT_DATE_SHORT);
  SET_STRING(IDS, LOCALE_MONTHS_SHORT);
  SET_STRING(IDS, LOCALE_DAYS_SHORT);

  SET_STRING(IDS_FILE_BROWSER, BODY_FONT_FAMILY);
  SET_STRING(IDS_FILE_BROWSER, BODY_FONT_SIZE);

  SET_STRING(IDS_FILE_BROWSER, ROOT_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, DOWNLOADS_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, MEDIA_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, NAME_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, SIZE_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, DATE_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, PREVIEW_COLUMN_LABEL);

  SET_STRING(IDS_FILE_BROWSER, ERROR_CREATING_FOLDER);
  SET_STRING(IDS_FILE_BROWSER, ERROR_INVALID_FOLDER_CHARACTER);
  SET_STRING(IDS_FILE_BROWSER, ERROR_INVALID_FILE_CHARACTER);
  SET_STRING(IDS_FILE_BROWSER, NEW_FOLDER_PROMPT);
  SET_STRING(IDS_FILE_BROWSER, NEW_FOLDER_BUTTON_LABEL);
  SET_STRING(IDS_FILE_BROWSER, FILENAME_LABEL);

  SET_STRING(IDS_FILE_BROWSER, EJECT_BUTTON);
  SET_STRING(IDS_FILE_BROWSER, IMAGE_DIMENSIONS);
  SET_STRING(IDS_FILE_BROWSER, VOLUME_LABEL);
  SET_STRING(IDS_FILE_BROWSER, READ_ONLY);

  SET_STRING(IDS_FILE_BROWSER, ERROR_RENAMING);
  SET_STRING(IDS_FILE_BROWSER, RENAME_PROMPT);
  SET_STRING(IDS_FILE_BROWSER, RENAME_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, ERROR_DELETING);
  SET_STRING(IDS_FILE_BROWSER, DELETE_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, ERROR_MOVING);
  SET_STRING(IDS_FILE_BROWSER, MOVE_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, ERROR_PASTING);
  SET_STRING(IDS_FILE_BROWSER, PASTE_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, COPY_BUTTON_LABEL);
  SET_STRING(IDS_FILE_BROWSER, CUT_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, DEVICE_TYPE_FLASH);
  SET_STRING(IDS_FILE_BROWSER, DEVICE_TYPE_HDD);
  SET_STRING(IDS_FILE_BROWSER, DEVICE_TYPE_OPTICAL);
  SET_STRING(IDS_FILE_BROWSER, DEVICE_TYPE_UNDEFINED);

  SET_STRING(IDS_FILE_BROWSER, CANCEL_LABEL);
  SET_STRING(IDS_FILE_BROWSER, OPEN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, SAVE_LABEL);

  SET_STRING(IDS_FILE_BROWSER, SELECT_FOLDER_TITLE);
  SET_STRING(IDS_FILE_BROWSER, SELECT_OPEN_FILE_TITLE);
  SET_STRING(IDS_FILE_BROWSER, SELECT_OPEN_MULTI_FILE_TITLE);
  SET_STRING(IDS_FILE_BROWSER, SELECT_SAVEAS_FILE_TITLE);

  SET_STRING(IDS_FILE_BROWSER, COMPUTING_SELECTION);
  SET_STRING(IDS_FILE_BROWSER, NOTHING_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, ONE_FILE_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, MANY_FILES_SELECTED);

  // FILEBROWSER, without the underscore, is from the old school codebase.
  // TODO(rginda): Move these into IDS_FILE_BROWSER post M12.
  SET_STRING(IDS_FILEBROWSER, CONFIRM_DELETE);

  SET_STRING(IDS_FILEBROWSER, ENQUEUE);
#undef SET_STRING

  // TODO(serya): Create a new string in .grd file for this one in M13.
  dict->SetString("PREVIEW_IMAGE",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_VIEW_CERT_BUTTON));
  dict->SetString("PLAY_MEDIA",
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLAY));

  return true;
}
