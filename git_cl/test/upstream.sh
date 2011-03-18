#!/bin/bash

# Check guessing the svn upstream branch.

set -e

. ./test-lib.sh

setup_initsvn
setup_gitsvn

(
  set -e
  cd git-svn

  git config rietveld.server localhost:8080

  for ref in refs/remotes/trunk refs/remotes/some_branch; do
    git checkout -q -B feature_branch $ref
    test_expect_success "Guessing upstream branch for $ref" \
        "$GIT_CL upstream | egrep -q '^$ref$'"
    git checkout -q master
  done
)

SUCCESS=$?

cleanup

if [ $SUCCESS == 0 ]; then
  echo PASS
fi
