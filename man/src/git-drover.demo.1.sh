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

echo "# Make sure we have the most up-to-date branch sources."
run git fetch
echo
echo "# Here's the commit we want to 'drover'."
run git log -n 1 --pretty=fuller
echo
echo "# Checkout the branch we want to 'drover' to."
run git checkout -b drover_9999 branch-heads/9999
echo
echo "# Now do the 'drover'."
echo "# IMPORTANT!!! Do Not leave off the '-x' flag"
run git cherry-pick -x $(git show-ref -s pick_commit)
echo
echo "# That took the code authored by some.commiter and commited it to the"
echo "# branch by branch.maintainer (us)."
run git log -n 1 --pretty=fuller
echo
echo "# Looks good. Ship it!"
pcommand git cl upload
echo "# Get LGTM or TBR."
run git cl land
echo "# Or skip the LGTM/TBR and just 'git cl land --bypass-hooks'"
