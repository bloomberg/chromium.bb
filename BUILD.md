# Get dependencies

## Python 2 & 3

Download and install the latest version of Python 2 & 3 for Windows from

    https://www.python.org/downloads/windows/

In order to support both python 2 and python 3 on the same computer, it is
recommended to make the following changes after installation:

    cp /c/dev/Python27/python.exe /c/dev/Python27/python2.exe
    cp /c/dev/Python39/python.exe /c/dev/Python39/python3.exe

and add both `/c/dev/Python27` and `/c/dev/Python39` to your path.

Note: As of CR94, python 2 will [no longer be needed](https://bugs.chromium.org/p/chromium/issues/detail?id=1205628). Also install Python 3.9 to build CR92. 3.10 doesn't work with it.

# Cloning the repo

For simplicity, let us assume the two repos are cloned into the directory `${WORK}`

    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

    git clone -c core.longpaths=true \
              -c core.deltaBaseCacheLimit=2g \
              https://github.com/bloomberg/chromium.bb.git

By default, Git for Windows uses the ANSI filesystem APIs that have a filepath
limit of 4096 characters.  Some files in the repository will exceed this limit
so this can be worked around by specifying `core.longpaths=true`, which tells
Git to use the Unicode filesystem API.

The `core.deltaBaseCacheLimit=2g` setting makes Git more efficient at handling
deltas in the chromium project, due to its size.  This setting is recommended
from the upstream chromium project.

# Setup environment variables

    export PATH="${WORK}/depot_tools:${PATH}"
    export DEPOT_TOOLS_WIN_TOOLCHAIN=0

# Get git hooks

There are a bunch of files that typically downloaded from Google and added to
the chromium working directory.  These files are needed to build chromium, but
they are not part of the source branches in this git repo.

In order to get these files into your working directory, we have some git hooks..
You should copy these hooks into your `chromium.bb/.git/hooks` directory.

The following commands can be used to install these hooks:

    cd ${WORK}/chromium.bb
    git show origin/hooks:post-checkout > .git/hooks/post-checkout
    chmod a+x .git/hooks/post-checkout

Now, whenever you change branches, it will update your working directory with
the files needed for the version you checked out.

Note that these hooks are only invoked when you use `git checkout` to change
the `HEAD` pointer.  There are other commands that may update the `HEAD`
pointer, but will not invoke these hooks (e.g. `git reset`, `git rebase`, etc).
If you use any of these commands, then you should manually invoke
`.git/hooks/post-checkout` to update your working directory with the correct
version of the non-git files.

# Switch to a release branch

Before proceeding, you can switch to a release branch in the `chromium.bb`
repo. This is an integration branch that merges all downstream feature
and bugfix branches. For `blpwtk2`, switch to a branch like
`m92/release/blpwtk2` and for `blpwebview`, switch to a branch like
`m98/release/blpwebview`.

# Install Windows SDK

Open the python script `${WORK}/chromium.bb/src/build/vs_toolchain.py` in a
plain text editor and read the comment right after all the `import` lines
(which should be around line 20). At the time of this writing, the version
of Windows SDK mentioned in this comment is `10.0.19041`.

Visit [Windows SDK Download](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/)
page to download this version of the SDK.

# Build instructions


    cd ${WORK}/chromium.bb/src
    third_party/bb_tools/gn.py   # Run this every time you switch major versions

This will generate several output directories, within which you can run `ninja`
to build the code.

There are three decisions you need to make before starting a build:

  1. Build static libraries or shared libraries
  2. Build in debug mode or release mode
  3. Build in 32-bit or 64-bit mode

Static builds are more performant during runtime but their linking time is quite long.
Debug builds include more debugging information into the build but produces much less efficient code compared to Release builds
64-bit builds require the embedder to be 64-bit as well.

Based on your decision, run **one** (or more) of the following commands:

    third_party/bb_tools/ninja -C out/shared_debug     [blpwtk2_all | blpwebview_all]
    third_party/bb_tools/ninja -C out/shared_release   [blpwtk2_all | blpwebview_all]
    third_party/bb_tools/ninja -C out/static_release   [blpwtk2_all | blpwebview_all]
    third_party/bb_tools/ninja -C out/shared_debug64   [blpwtk2_all | blpwebview_all]
    third_party/bb_tools/ninja -C out/shared_release64 [blpwtk2_all | blpwebview_all]
    third_party/bb_tools/ninja -C out/static_release64 [blpwtk2_all | blpwebview_all]

Static debug configuration is not supported due to a size limitation of PDB files.

Note: You may use another `ninja` from your `PATH`, however, it is recommended
to use a version of `ninja` that is known to work with the version of chromium
that you have checked out, hence the explicit path being used above.

You can save time when running `third_party/bb_tools/gn.py` by making it skip
configurations that you do not need, but adding one or more of the following
flags:

    --no-debug      Suppress generation of:            Equivalent to: --release-only
                        out/shared_debug
                        out/shared_debug64

    --no-release    Suppress generation of:            Equivalent to: --debug-only
                        out/shared_release
                        out/static_release
                        out/shared_release64
                        out/static_release64

    --no-shared    Suppress generation of:             Equivalent to: --static-only
                        out/shared_debug
                        out/shared_release
                        out/shared_debug64
                        out/shared_release64

    --no-static    Suppress generation of:             Equivalent to: --shared-only
                        out/static_release
                        out/static_release64

    --no-x86       Suppress generation of:             Equivalent to: --x64-only
                        out/shared_debug
                        out/shared_release
                        out/static_release

    --no-x64       Suppress generation of:             Equivalent to: --x86-only
                        out/shared_debug64
                        out/shared_release64
                        out/static_release64

For example, to generate only `out/shared_release` and `out/shared_release64`,
you can use:

    third_party/bb_tools/gn.py --no-static --no-debug
                                                                   or:
    third_party/bb_tools/gn.py --shared-only --release-only


