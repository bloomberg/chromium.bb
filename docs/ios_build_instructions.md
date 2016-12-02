# Checking out and building Chromium for iOS

There are instructions for other platforms linked from the 
[get the code](get_the_code.md) page.

## Instructions for Google Employees

Are you a Google employee? See
[go/building-chrome](https://goto.google.com/building-chrome) instead.
Google employee? See [go/building-chrome](https://goto.google.com/building-chrome) instead.

[TOC]

## System requirements

* A 64-bit Mac running 10.11+.
* [Xcode](https://developer.apple.com/xcode) 8.0+.
* The OS X 10.10 SDK. Run

    ```shell  
    $ ls `xcode-select -p`/Platforms/MacOSX.platform/Developer/SDKs
    ```
 
  to check whether you have it.  Building with the 10.11 SDK works too, but
  the releases currently use the 10.10 SDK.
* The current version of the JDK (required for the Closure compiler).

## Install `depot_tools`

Clone the `depot_tools` repository:

```shell
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

Add `depot_tools` to the end of your PATH (you will probably want to put this
in your `~/.bashrc` or `~/.zshrc`). Assuming you cloned `depot_tools` to
`/path/to/depot_tools`:

```shell
$ export PATH="$PATH:/path/to/depot_tools"
```

## Get the code

Create a `chromium` directory for the checkout and change to it (you can call
this whatever you like and put it wherever you like, as
long as the full path has no spaces):

```shell
$ mkdir chromium && cd chromium
```

Run the `fetch` tool from `depot_tools` to check out the code and its
dependencies.

```shell
$ fetch ios
```

If you don't want the full repo history, you can save a lot of time by
adding the `--no-history` flag to `fetch`.

Expect the command to take 30 minutes on even a fast connection, and many
hours on slower ones.

When `fetch` completes, it will have created a hidden `.gclient` file and a
directory called `src` in the working directory. The remaining instructions
assume you have switched to the `src` directory:

```shell
$ cd src
```

*Optional*: You can also [install API
keys](https://www.chromium.org/developers/how-tos/api-keys) if you want your
build to talk to some Google services, but this is not necessary for most
development and testing purposes.

## Setting up the build

Since the iOS build is a bit more complicated than a desktop build, we provide
`ios/build/tools/setup-gn.py`, which will create four appropriately configured
build directories under `out` for Release and Debug device and simulator
builds, and generates an appropriate Xcode workspace as well. 

This script is run automatically by fetch (as part of `gclient runhooks`).

You can customize the build by editing the file `$HOME/.setup-gn` (create it if
it does not exist).  Look at `src/ios/build/tools/setup-gn.config` for
available configuration options.

From this point, you can either build from Xcode or from the command line using
`ninja`. `setup-gn.py` creates sub-directories named
`out/${configuration}-${platform}`, so for a `Debug` build for simulator use:

```shell
$ ninja -C out/Debug-iphonesimulator gn_all
```

Note: you need to run `setup-gn.py` script every time one of the `BUILD.gn`
file is updated (either by you or after rebasing). If you forget to run it,
the list of targets and files in the Xcode solution may be stale.

You can also follow the manual instructions on the 
[Mac page](mac_build_instructions.md), but make sure you set the
GN arg `target_os="ios"`.

## Running `ios_web_shell`

Any target that is built and runs on the bots (see [below](#Troubleshooting))
should run successfully in a local build. As of the time of writing, this is
only the `ios_web_shell` and `ios_chrome_unittests` targets—see the note at the
top of this page. Check the bots periodically for updates; more targets (new
components) will come on line over time.

To run in the simulator from the command line, you can use `iossim`. For
example, to run a debug build of `ios_web_shell`:

```shell
$ out/Debug-iphonesimulator/iossim out/Debug-iphonesimulator/ios_web_shell.app
```

## Update your checkout

To update an existing checkout, you can run

```shell
$ git rebase-update
$ gclient sync
```

The first command updates the primary Chromium source repository and rebases
any of your local branches on top of tip-of-tree (aka the Git branch
`origin/master`). If you don't want to use this script, you can also just use
`git pull` or other common Git commands to update the repo.

The second command syncs dependencies to the appropriate versions and re-runs
hooks as needed.

## Tips, tricks, and troubleshooting

If you have problems building, join us in `#chromium` on `irc.freenode.net` and
ask there. As mentioned above, be sure that the
[waterfall](http://build.chromium.org/buildbot/waterfall/) is green and the tree
is open before checking out. This will increase your chances of success.

### Improving performance of `git status`

`git status` is used frequently to determine the status of your checkout.  Due
to the large number of files in Chromium's checkout, `git status` performance
can be quite variable.  Increasing the system's vnode cache appears to help.
By default, this command:

```shell
$ sysctl -a | egrep kern\..*vnodes
```

Outputs `kern.maxvnodes: 263168` (263168 is 257 * 1024).  To increase this
setting:

```shell
$ sudo sysctl kern.maxvnodes=$((512*1024))
```

Higher values may be appropriate if you routinely move between different
Chromium checkouts.  This setting will reset on reboot, the startup setting can
be set in `/etc/sysctl.conf`:

```shell
$ echo kern.maxvnodes=$((512*1024)) | sudo tee -a /etc/sysctl.conf
```

Or edit the file directly.

If `git --version` reports 2.6 or higher, the following may also improve
performance of `git status`:

```shell
$ git update-index --untracked-cache
```

### Xcode license agreement

If you're getting the error

> Agreeing to the Xcode/iOS license requires admin privileges, please re-run as
> root via sudo.

the Xcode license hasn't been accepted yet which (contrary to the message) any
user can do by running:

```shell
$ xcodebuild -license
```

Only accepting for all users of the machine requires root:

```shell
$ sudo xcodebuild -license
```
