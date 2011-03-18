#!/bin/bash

set -e

. ./test-lib.sh

setup_initsvn
setup_gitsvn

(
  set -e
  cd git-svn
  git checkout -q -b work
  echo "some work done on a branch" >> test
  git add test; git commit -q -m "branch work"

  git config rietveld.server localhost:8080

  # Prevent the editor from coming up when you upload.
  export EDITOR=$(which true)

  test_expect_success "upload succeeds (needs a server running on localhost)" \
    "$GIT_CL upload -m test master | grep -q 'Issue created'"

  test_expect_success "git-cl status now knows the issue" \
    "$GIT_CL status | grep -q 'Issue number'"

  ISSUE=$($GIT_CL status | awk '$0 ~ "Issue number:" { print $3 }')

  git checkout -q -b test2 master

  test_expect_success "git cl patch $ISSUE"
)
SUCCESS=$?

cleanup

if [ $SUCCESS == 0 ]; then
  echo PASS
fi
