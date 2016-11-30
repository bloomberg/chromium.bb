# Checking out and building Chromium on Linux

**See also [the old version of this page](old_linux_build_instructions.md).**

Google employee? See [go/building-chrome](https://goto.google.com/building-chrome) instead.

[TOC]

## System requirements

*   A 64-bit Intel machine with at least 8GB of RAM. More than 16GB is highly
    recommended.
*   At least 100GB of free disk space.
*   You must have Git and Python installed already.

Most development is done on Ubuntu (currently 14.04, Trusty Tahr).  There are
some instructions for other distros below, but they are mostly unsupported.

## Install `depot_tools`

Clone the depot_tools repository:

    $ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

Add depot_tools to the end of your PATH (you will probably want to put this
in your ~/.bashrc or ~/.zshrc). Assuming you cloned depot_tools
to /path/to/depot_tools:

    $ export PATH=$PATH:/path/to/depot_tools

## Get the code

Create a chromium directory for the checkout and change to it (you can call
this whatever you like and put it wherever you like, as
long as the full path has no spaces):
    
    $ mkdir chromium
    $ cd chromium

Run the `fetch` tool from depot_tools to check out the code and its
dependencies.

    $ fetch --nohooks chromium

If you don't want the full repo history, you can save a lot of time by
adding the `--no-history` flag to fetch.

Expect the command to take 30 minutes on even a fast connection, and many
hours on slower ones.

If you've already installed the build dependencies on the machine (from another
checkout, for example), you can omit the `--nohooks` flag and fetch
will automatically execute `gclient runhooks` at the end.

When fetch completes, it will have created a directory called `src`.
The remaining instructions assume you are now in that directory:

    $ cd src

### Install additional build dependencies

Once you have checked out the code, and assuming you're using Ubuntu, run
[build/install-build-deps.sh](/build/install-build-deps.sh)

Here are some instructions for what to do instead for

* [Debian](linux_debian_build_instructions.md)
* [Fedora](linux_fedora_build_instructions.md)
* [Arch Linux](linux_arch_build_instructions.md)
* [Open SUSE](linux_open_suse_build_instrctions.md)
* [Mandriva](linux_mandriva_build_instrctions.md)

For Gentoo, you can just run `emerge www-client/chromium`.

### Run the hooks

Once you've run `install-build-deps` at least once, you can now run the
chromium-specific hooks, which will download additional binaries and other
things you might need:

    $ gclient runhooks

*Optional*: You can also [install API keys](https://www.chromium.org/developers/how-tos/api-keys)
if you want to talk to some of the Google services, but this is not necessary
for most development and testing purposes.

## Seting up the Build

Chromium uses [Ninja](https://ninja-build.org) as its main build tool, and
a tool called [GN](../tools/gn/docs/quick_start.md) to generate
the .ninja files to do the build. To create a build directory, run:

    $ gn gen out/Default

* You only have to do run this command once, it will self-update the build
  files as needed after that.
* You can replace `out/Default` with another directory name, but we recommend
  it should still be a subdirectory of `out`.
* To specify build parameters for GN builds, including release settings,
  see [GN build configuration](https://www.chromium.org/developers/gn-build-configuration). 
  The default will be a debug component build matching the current host
  operating system and CPU.
* For more info on GN, run `gn help` on the command line or read the
  [quick start guide](../tools/gn/docs/quick_start.md).

### Faster builds

See [faster builds on Linux](linux_faster_builds.md) for various tips and
settings that may speed up your build.

## Build Chromium

Build Chromium (the "chrome" target) with Ninja using the command:

    $ ninja -C out/Default chrome

You can get a list of all of the other build targets from GN by running
`gn ls out/Default` from the command line. To compile one, pass to Ninja
the GN label with no preceding "//" (so for `//chrome/test:unit_tests`
use ninja -C out/Default chrome/test:unit_tests`).

## Run Chromium

Once it is built, you can simply run the browser:

    $ out/Default/chrome

## Running test targets

You can run the tests in the same way. You can also limit which tests are
run using the `--gtest_filter` arg, e.g.:

    $ ninja -C out/Default unit_tests --gtest_filter="PushClientTest.*"

You can find out more about GoogleTest at its
[GitHub page](https://github.com/google/googletest).

## Update your checkout

To update an existing checkout, you can run

    $ git rebase-update
    $ gclient sync

The first command updates the primary Chromium source repository and rebases
any of your local branches on top of tip-of-tree (aka the Git branch `origin/master`).
If you don't want to use this script, you can also just use `git pull` or 
other common Git commands to update the repo.

The second command syncs the subrepositories to the appropriate versions and
re-runs the hooks as needed.

## Tips, tricks, and troubleshooting

### Linker Crashes

If, during the final link stage:

    LINK out/Debug/chrome

You get an error like:

    collect2: ld terminated with signal 6 Aborted terminate called after throwing an
    instance of 'std::bad_alloc'

    collect2: ld terminated with signal 11 [Segmentation fault], core dumped

you are probably running out of memory when linking. You *must* use a 64-bit
system to build. Try the following build settings (see [GN build
configuration](https://www.chromium.org/developers/gn-build-configuration) for
setting):

*   Build in release mode (debugging symbols require more memory).
    `is_debug = false`
*   Turn off symbols. `symbol_level = 0`
*   Build in component mode (this is for developers only, it will be slower and
    may have broken functionality). `is_component_build = true`

### More links

*   Information about [building with Clang](clang.md).
*   You may want to [use a chroot](using_a_linux_chroot.md) to
    isolate yourself from versioning or packaging conflicts.
*   Cross-compiling for ARM? See [LinuxChromiumArm](linux_chromium_arm.md).
*   Want to use Eclipse as your IDE? See
    [LinuxEclipseDev](linux_eclipse_dev.md).
*   Want to use your built version as your default browser? See
    [LinuxDevBuildAsDefaultBrowser](linux_dev_build_as_default_browser.md).

### Next Steps

If you want to contribute to the effort toward a Chromium-based browser for
Linux, please check out the [Linux Development page](linux_development.md) for
more information.
