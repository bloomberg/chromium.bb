This package contains tools for working with Chromium development:

  chrome-update-create-task.bat
    Creates a scheduled task to do an automatic local chromium build every day.

  cpplint.py
    A copy of our linting tool which enforces Google style. Fetched from
    http://google-styleguide.googlecode.com/svn/trunk/cpplint/cpplint.py

  gcl
    A tool for uploading and managing code reviews on the Chromium
    project, using the Rietveld code review tool.  More info at:
    http://code.google.com/p/rietveld/

  gclient
    A script for managing a workspace with modular dependencies that
    are each checked out independently from different repositories.
    More info at:
    http://code.google.com/p/gclient/

It updates itself automatically when running `gclient` tool. To disable
auto update, set the environment variable DEPOT_TOOLS_UPDATE=0

To update package manually, run .\update_depot_tools.bat on Windows,
or ./update_depot_tools on Linux or Mac.

Note: on Windows if svn, git and python are not accessible, they will be
downloaded too.


## Contributing

The "gclient" wrapper knows how to keep this repository updated to
the latest versions of these tools as found at:

    https://chromium.googlesource.com/chromium/tools/depot_tools.git

To contribute change for review:

    git new-branch <somename>
    git add <yourchanges>
    git commit
    # find reviewers
    git cl owners
    git log <yourfiles>
    # upload
    git cl upload -r reviewer1@chromium.org,reviewer2 --send-mail
    # open https://codereview.chromium.org/ and send mail

    # if change is approved, flag it to be commited
    git cl set_commit
    # if change needs more work
    git rebase-update
    ...
    git cl upload
