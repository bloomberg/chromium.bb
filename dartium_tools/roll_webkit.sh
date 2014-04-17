#!/bin/bash -e

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script does WebKit roll provided old and new svn revisions.

gitSha() {
  git log --format=%h --grep "git-svn-id: svn://svn.chromium.org/blink/trunk@${1}" blink/master
}

git checkout master
git svn rebase
git fetch blink

old_svn_rev=$1
new_svn_rev=$2

old_rev="$(gitSha $old_svn_rev)"
new_rev="$(gitSha $new_svn_rev)"

merge_branch_name="merge-${old_svn_rev}-${new_svn_rev}"

git checkout -b ${merge_branch_name} ${old_rev}
git diff ${old_rev} ${new_rev} --binary | git apply --binary --index
# git cherry-pick --no-commit ${old_rev}..${new_rev}
git commit -m "MERGE: ${old_svn_rev}-${new_svn_rev}."
git rebase --onto master ${merge_branch_name}~1 ${merge_branch_name}
 