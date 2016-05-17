# Windows binary tool bootstrap

This directory has the 'magic' for the `depot_tools` windows binary update
mechanisms.

## Software bootstrapped
  * Python (https://www.python.org/)
  * Git for Windows (https://git-for-windows.github.io/)
  * Subversion (https://subversion.apache.org/)

## Mechanism

Any time a user runs `gclient` on windows, it will invoke the `depot_tools`
autoupdate script [depot_tools.bat](../../update_depot_tools.bat). This, in
turn, will run [win_tools.bat](./win_tools.bat), which does the bulk of the
work.

`win_tools.bat` will successively look to see if the local version of the binary
package is present, and if so, if it's the expected version. If either of those
cases is not true, it will download and unpack the respective binary.

Downloading is done with [get_file.js](./get_file.js), which is a windows script
host javascript utility to vaguely impersonate `wget`.

Through a comedy of history, each binary is stored and retrieved differently.

### Git

Git installs are mirrored versions of the offical Git-for-Windows Portable
releases.
  * Original: `https://github.com/git-for-windows/git/releases`
  * Mirror: `gs://chrome-infra/PortableGit*.7z.exe`

#### Updating git version
  1. Download the new `PortableGit-X.Y.Z-{32,64}.7z.exe` from the
     git-for-windows release page.
  1. From either console.developers.google.com or the CLI, do:
    1. Upload those to the gs://chrome-infra Google Storage bucket.
    1. Set the `allUsers Reader` permission (click the "Public link" checkbox
       next to the binaries).
  1. Edit the `set GIT_VERSION=X.Y.Z` line in `win_tools.bat` to be the new
     version.
    1. At the time of writing, the first version is the default version, and
       the second is the 'bleeding edge' version. You can use the bleeding edge
       version to get early feedback/stage a rollout/etc.
  1. Commit the CL.

Note that in order for the update to take effect, `gclient` currently needs to
run twice. The first time it will update the `depot_tools` repo, and the second
time it will see the new git version and update to it. This is a bug that should
be fixed, in case you're reading this and this paragraph infuriates you more
than the rest of this README.

### Python

Python installs are sourced from gs://chrome-infra/python276_bin.zip .

The process to create them is sort-of-documented in the README of the python
zip file.

### Subversion

Subversion installs are sourced from gs://chrome-infra/svn_bin.zip .

There will likely never be an update to SVN in `depot_tools` from the current
version.
