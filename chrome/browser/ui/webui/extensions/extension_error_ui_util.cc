// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_error_ui_util.h"

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/file_highlighter.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace extensions {
namespace error_ui_util {

namespace {

// Keys for objects passed to and from extension error UI.
const char kPathSuffixKey[] = "pathSuffix";
const char kTitleKey[] = "title";

std::string ReadFileToString(const base::FilePath& path) {
  std::string data;
  base::ReadFileToString(path, &data);
  return data;
}

void GetManifestFileCallback(base::DictionaryValue* results,
                             const std::string& key,
                             const std::string& specific,
                             const RequestFileSourceCallback& response,
                             const std::string& contents) {
  ManifestHighlighter highlighter(contents, key, specific);
  highlighter.SetHighlightedRegions(results);
  response.Run(*results);
}

void GetSourceFileCallback(base::DictionaryValue* results,
                           int line_number,
                           const RequestFileSourceCallback& response,
                           const std::string& contents) {
  SourceHighlighter highlighter(contents, line_number);
  highlighter.SetHighlightedRegions(results);
  response.Run(*results);
}

}  // namespace

void HandleRequestFileSource(const base::DictionaryValue* args,
                             Profile* profile,
                             const RequestFileSourceCallback& response) {
  // Three required arguments: extension_id, path_suffix, and error_message.
  std::string extension_id;
  base::FilePath::StringType path_suffix_string;
  base::string16 error_message;

  if (!args->GetString(kPathSuffixKey, &path_suffix_string) ||
      !args->GetString(ExtensionError::kExtensionIdKey, &extension_id) ||
      !args->GetString(ExtensionError::kMessageKey, &error_message)) {
    NOTREACHED();
    return;
  }

  const Extension* extension =
      ExtensionRegistry::Get(profile)->GetExtensionById(
          extension_id, ExtensionRegistry::EVERYTHING);

  if (!extension) {
    NOTREACHED();
    return;
  }

  // Under no circumstances should we ever need to reference a file outside of
  // the extension's directory. If it tries to, abort.
  base::FilePath path_suffix(path_suffix_string);
  if (path_suffix.ReferencesParent())
    return;

  base::FilePath path = extension->path().Append(path_suffix);

  // Setting the title and the error message is the same for all file types.
  scoped_ptr<base::DictionaryValue> results(new base::DictionaryValue);
  results->SetString(kTitleKey,
                     base::UTF8ToUTF16(extension->name()) +
                         base::ASCIIToUTF16(": ") +
                         path.BaseName().LossyDisplayName());
  results->SetString(ExtensionError::kMessageKey, error_message);

  base::Callback<void(const std::string&)> reply;
  if (path_suffix_string == kManifestFilename) {
    std::string manifest_key;
    if (!args->GetString(ManifestError::kManifestKeyKey, &manifest_key)) {
      NOTREACHED();
      return;
    }

    // A "specific" location is optional.
    std::string specific;
    args->GetString(ManifestError::kManifestSpecificKey, &specific);

    reply = base::Bind(&GetManifestFileCallback,
                       base::Owned(results.release()),
                       manifest_key,
                       specific,
                       response);
  } else {
    int line_number = 0;
    args->GetInteger(RuntimeError::kLineNumberKey, &line_number);

    reply = base::Bind(&GetSourceFileCallback,
                       base::Owned(results.release()),
                       line_number,
                       response);
  }

  base::PostTaskAndReplyWithResult(content::BrowserThread::GetBlockingPool(),
                                   FROM_HERE,
                                   base::Bind(&ReadFileToString, path),
                                   reply);
}

void HandleOpenDevTools(const base::DictionaryValue* args) {
  int render_process_id = 0;
  int render_view_id = 0;

  // The render view and render process ids are required.
  if (!args->GetInteger(RuntimeError::kRenderProcessIdKey,
                        &render_process_id) ||
      !args->GetInteger(RuntimeError::kRenderViewIdKey, &render_view_id)) {
    NOTREACHED();
    return;
  }

  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(render_process_id, render_view_id);

  // It's possible that the render view was closed since we last updated the
  // links. Handle this gracefully.
  if (!rvh)
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  if (!web_contents)
    return;

  // If we include a url, we should inspect it specifically (and not just the
  // render view).
  base::string16 url;
  if (args->GetString(RuntimeError::kUrlKey, &url)) {
    // Line and column numbers are optional; default to the first line.
    int line_number = 1;
    int column_number = 1;
    args->GetInteger(RuntimeError::kLineNumberKey, &line_number);
    args->GetInteger(RuntimeError::kColumnNumberKey, &column_number);

    // Line/column numbers are reported in display-friendly 1-based numbers,
    // but are inspected in zero-based numbers.
    DevToolsWindow::OpenDevToolsWindow(
        web_contents,
        DevToolsToggleAction::Reveal(url, line_number - 1, column_number - 1));
  } else {
    DevToolsWindow::OpenDevToolsWindow(web_contents);
  }

  // Once we open the inspector, we focus on the appropriate tab...
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);

  // ... but some pages (popups and apps) don't have tabs, and some (background
  // pages) don't have an associated browser. For these, the inspector opens in
  // a new window, and our work is done.
  if (!browser || !browser->is_type_tabbed())
    return;

  TabStripModel* tab_strip = browser->tab_strip_model();
  tab_strip->ActivateTabAt(tab_strip->GetIndexOfWebContents(web_contents),
                           false);  // Not through direct user gesture.
}

}  // namespace error_ui_util
}  // namespace extensions
