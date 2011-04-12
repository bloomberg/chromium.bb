// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_selection_policy.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

#if !defined(OS_CHROMEOS)
#error This file is meant to be compiled on ChromeOS only.
#endif

using std::vector;
using std::string;
using std::pair;
using std::map;

namespace chromeos {

static const char kPluginSelectionPolicyFile[] =
    "/usr/share/chromeos-assets/flash/plugin_policy";

PluginSelectionPolicy::PluginSelectionPolicy()
    : init_from_file_finished_(false) {
}

void PluginSelectionPolicy::StartInit() {
  // Initialize the policy on the FILE thread, since it reads from a
  // policy file.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &chromeos::PluginSelectionPolicy::Init));
}

bool PluginSelectionPolicy::Init() {
  return InitFromFile(FilePath(kPluginSelectionPolicyFile));
}

bool PluginSelectionPolicy::InitFromFile(const FilePath& policy_file) {
  // This must always be called from the FILE thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  string data;
  // This should be a really small file, so we're OK with just
  // slurping it.
  if (!file_util::ReadFileToString(policy_file, &data)) {
    LOG(ERROR) << "Unable to read plugin policy file \""
               << policy_file.value() << "\".";
    init_from_file_finished_ = true;
    return false;
  }

  std::istringstream input_stream(data);
  string line;
  map<string, Policy> policies;
  Policy policy;
  string last_plugin;

  while (std::getline(input_stream, line)) {
    // Strip comments.
    string::size_type pos = line.find("#");
    if (pos != string::npos) {
      line = line.substr(0, pos);
    }
    TrimWhitespaceASCII(line, TRIM_ALL, &line);
    if (line.find("allow") == 0) {
      // Has to be preceeded by a "plugin" statement.
      if (last_plugin.empty()) {
        LOG(ERROR) << "Plugin policy file error: 'allow' out of context.";
        init_from_file_finished_ = true;
        return false;
      }
      line = line.substr(5);
      TrimWhitespaceASCII(line, TRIM_ALL, &line);
      line = StringToLowerASCII(line);
      policy.push_back(make_pair(true, line));
    }
    if (line.find("deny") == 0) {
      // Has to be preceeded by a "plugin" statement.
      if (last_plugin.empty()) {
        LOG(ERROR) << "Plugin policy file error: 'deny' out of context.";
        init_from_file_finished_ = true;
        return false;
      }
      line = line.substr(4);
      TrimWhitespaceASCII(line, TRIM_ALL, &line);
      line = StringToLowerASCII(line);
      policy.push_back(make_pair(false, line));
    }
    if (line.find("plugin") == 0) {
      line = line.substr(6);
      TrimWhitespaceASCII(line, TRIM_ALL, &line);
      if (!policy.empty() && !last_plugin.empty())
        policies.insert(make_pair(last_plugin, policy));
      last_plugin = line;
      policy.clear();
    }
  }

  if (!last_plugin.empty())
    policies.insert(make_pair(last_plugin, policy));

  policies_.swap(policies);
  init_from_file_finished_ = true;
  return true;
}

int PluginSelectionPolicy::FindFirstAllowed(
    const GURL& url,
    const std::vector<webkit::npapi::WebPluginInfo>& info) {
  for (std::vector<webkit::npapi::WebPluginInfo>::size_type i = 0;
       i < info.size(); ++i) {
    if (IsAllowed(url, info[i].path))
      return i;
  }
  return -1;
}

bool PluginSelectionPolicy::IsAllowed(const GURL& url,
                                      const FilePath& path) {
  // This must always be called from the FILE thread, to be sure
  // initialization doesn't happen at the same time.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Make sure that we notice if this starts being called before
  // initialization is complete.  Right now it is guaranteed only by
  // the startup order and the fact that InitFromFile runs on the FILE
  // thread too.
  DCHECK(init_from_file_finished_)
      << "Tried to check policy before policy is initialized.";

  string name = path.BaseName().value();

  PolicyMap::iterator policy_iter = policies_.find(name);
  if (policy_iter != policies_.end()) {
    Policy& policy(policy_iter->second);

    // We deny by default. (equivalent to "deny" at the top of the section)
    bool allow = false;

    for (Policy::iterator iter = policy.begin(); iter != policy.end(); ++iter) {
      bool policy_allow = iter->first;
      string& policy_domain = iter->second;
      if (policy_domain.empty() || url.DomainIs(policy_domain.c_str(),
                                                policy_domain.size())) {
        allow = policy_allow;
      }
    }
    return allow;
  }

  // If it's not in the policy file, then we assume it's OK to allow
  // it.
  return true;
}

}  // namespace chromeos
