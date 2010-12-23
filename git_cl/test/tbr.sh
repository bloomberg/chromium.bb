#!/bin/bash

# Tests the "tbr" functionality, which lets you submit without uploading
# first.

set -e

. ./test-lib.sh

setup_initsvn
setup_gitsvn

(
  set -e
  cd git-svn

  # We need a server set up, but we don't use it.
  git config rietveld.server localhost:1

  echo "some work done" >> test
  git add test; git commit -q -m "work"

  test_expect_success "git-cl dcommit tbr without an issue" \
    "$GIT_CL dcommit -f --tbr -m 'foo-quux'"

  git svn -q rebase >/dev/null 2>&1
  test_expect_success "dcommitted code has proper description" \
      "git show | grep -q 'foo-quux'"

  test_expect_success "upstream svn has our commit" \
      "svn log $REPO_URL 2>/dev/null | grep -q 'foo-quux'"
)
SUCCESS=$?

cleanup

if [ $SUCCESS == 0 ]; then
  echo PASS
fi
