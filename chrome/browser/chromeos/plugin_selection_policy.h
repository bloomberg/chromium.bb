// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_SELECTION_POLICY_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_SELECTION_POLICY_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "webkit/plugins/npapi/webplugininfo.h"

class GURL;
class FilePath;

namespace chromeos {

#if !defined(OS_CHROMEOS)
#error This file is meant to be compiled on ChromeOS only.
#endif

// This class is used to provide logic for selection of a plugin
// executable path in the browser.  It loads a policy file for
// selection of particular plugins based on the domain they are be
// instantiated for.  It is used by the PluginService.  It is (and
// should be) only used for ChromeOS.

// The functions in this class must only be called on the FILE thread
// (and will DCHECK if they aren't).

class PluginSelectionPolicy
    : public base::RefCountedThreadSafe<PluginSelectionPolicy> {
 public:
  PluginSelectionPolicy();

  // This should be called before any other method.  This starts the
  // process of initialization on the FILE thread.
  void StartInit();

  // Returns the first allowed plugin in the given vector of plugin
  // information.  Returns -1 if no plugins in the info vector are
  // allowed (or if the info vector is empty).  InitFromFile must
  // complete before any calls to FindFirstAllowed happen or it will
  // assert.
  int FindFirstAllowed(const GURL& url,
                       const std::vector<webkit::npapi::WebPluginInfo>& info);

  // Applies the current policy to the given path using the url to
  // look up what the policy for that domain is.  Returns true if the
  // given plugin is allowed for that domain.  InitFromFile must
  // complete before any calls to IsAllowed happen or it will assert.
  bool IsAllowed(const GURL& url, const FilePath& path);

 private:
  // To allow access to InitFromFile
  FRIEND_TEST_ALL_PREFIXES(PluginSelectionPolicyTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(PluginSelectionPolicyTest, FindFirstAllowed);
  FRIEND_TEST_ALL_PREFIXES(PluginSelectionPolicyTest, InitFromFile);
  FRIEND_TEST_ALL_PREFIXES(PluginSelectionPolicyTest, IsAllowed);
  FRIEND_TEST_ALL_PREFIXES(PluginSelectionPolicyTest, MissingFile);

  // Initializes from the default policy file.
  bool Init();

  // Initializes from the given file.
  bool InitFromFile(const FilePath& policy_file);

  typedef std::vector<std::pair<bool, std::string> > Policy;
  typedef std::map<std::string, Policy> PolicyMap;

  PolicyMap policies_;

  // This is used to DCHECK if we try and call IsAllowed or
  // FindFirstAllowed before we've finished executing InitFromFile.
  // Note: We're "finished" even if loading the file fails -- the
  // point of the DCHECK is to make sure we haven't violated our
  // ordering/threading assumptions, not to make sure that we're
  // properly initialized.
  bool init_from_file_finished_;

  DISALLOW_COPY_AND_ASSIGN(PluginSelectionPolicy);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_SELECTION_POLICY_H_
