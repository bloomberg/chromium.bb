#!/usr/bin/env bash
. demo_repo.sh

silent git push origin refs/remotes/origin/master:refs/branch-heads/9999
silent git config --add remote.origin.fetch \
  +refs/branch-heads/*:refs/remotes/branch-heads/*
silent git fetch origin

silent git checkout -b master origin/master
add modified_file
set_user some.committer
c "This change needs to go to branch 9999"
silent git tag pick_commit

comment Before working with branches, you must \'gclient sync \
  --with_branch_heads\' at least once to fetch the branches.

set_user branch.maintainer
tick 1000

run git log -n 1 --pretty=fuller
run git checkout -b drover_9999 branch-heads/9999
echo "# DO NOT leave off the '-x' flag"
run git cherry-pick -x $(git show-ref -s pick_commit)
run git log -n 1 --pretty=fuller
pcommand git cl upload
echo "# Get LGTM or TBR."
run git cl land
