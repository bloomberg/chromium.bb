#!/bin/bash

# Tests the "preupload and predcommit hooks" functionality, which lets you run
# hooks by installing a script into .git/hooks/pre-cl-* first.

set -e

. ./test-lib.sh

setup_initsvn
setup_gitsvn

(
  set -e
  cd git-svn

  # We need a server set up, but we don't use it.
  git config rietveld.server localhost:1

  # Install a pre-cl-upload hook.
  echo "#!/bin/bash" > .git/hooks/pre-cl-upload
  echo "echo 'sample preupload fail'" >> .git/hooks/pre-cl-upload
  echo "exit 1" >> .git/hooks/pre-cl-upload
  chmod 755 .git/hooks/pre-cl-upload

  # Install a pre-cl-dcommit hook.
  echo "#!/bin/bash" > .git/hooks/pre-cl-dcommit
  echo "echo 'sample predcommit fail'" >> .git/hooks/pre-cl-dcommit
  echo "exit 1" >> .git/hooks/pre-cl-dcommit
  chmod 755 .git/hooks/pre-cl-dcommit

  echo "some work done" >> test
  git add test; git commit -q -m "work"

  # Verify git cl upload fails.
  test_expect_failure "git-cl upload hook fails" "$GIT_CL upload master"

  # Verify git cl upload fails.
  test_expect_failure "git-cl dcommit hook fails" "$GIT_CL dcommit master"
)
SUCCESS=$?

#cleanup

if [ $SUCCESS == 0 ]; then
  echo PASS
fi
