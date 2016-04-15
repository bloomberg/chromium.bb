# Windows Build Instructions

## Setting up Windows

You must set your Windows system locale to English, or else you may get
build errors about "The file contains a character that cannot be
represented in the current code page."

### Setting up the environment for Visual Studio

You must build with Visual Studio 2015 Update 2; no other version is
supported.

You must have Windows 7 x64 or later. x86 OSs are unsupported.

## Step 1: Getting depot_tools

Get [depot\_tools](http://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up).

## Step 2: Getting the compiler toolchain

Follow the appropriate path below:

### Open source contributors

As of March 11, 2016 Chromium requires Visual Studio 2015 to build.

Install Visual Studio 2015 Update 2 or later - Community Edition
should work if its license is appropriate for you. Use the Custom Install option
and select:

- Visual C++, which will select three sub-categories including MFC
- Universal Windows Apps Development Tools > Tools
- Universal Windows Apps Development Tools > Windows 10 SDK (10.0.10586)

You must have the 10586 SDK installed or else you will hit compile errors such
as redefined macros.

Run `set DEPOT_TOOLS_WIN_TOOLCHAIN=0`, or set that variable in your
global environment.

Compilation is done through ninja, **not** Visual Studio.

### Google employees

Run: `download_from_google_storage --config` and follow the
authentication instructions. **Note that you must authenticate with your
@google.com credentials**, not @chromium.org. Enter "0" if asked for a
project-id.

Once you've done this, the toolchain will be installed automatically for
you in Step 3, below (near the end of the step).

The toolchain will be in `depot_tools\win_toolchain\vs_files\<hash>`, and windbg
can be found in `depot_tools\win_toolchain\vs_files\<hash>\win_sdk\Debuggers`.

If you want the IDE for debugging and editing, you will need to install
it separately, but this is optional and not needed to build Chromium.

## Step 3: Getting the Code

Follow the steps to [check out the
code](https://www.chromium.org/developers/how-tos/get-the-code) (largely
`fetch chromium`).

## Step 4: Building

Build the target you are interested in.

```shell
ninja -C out\Debug chrome
```

Alternative (Graphical user interface): Open a generated .sln
file such as chrome.sln, right-click the chrome project and select build.
This will invoke the real step 4 above. Do not build the whole solution
since that conflicts with ninja's build management and everything will
explode.
Substitute the build directory given to `-C` with `out\Debug_x64` for
[64-bit
builds](https://www.chromium.org/developers/design-documents/64-bit-support)
in GYP, or whatever build directory you have configured if using GN.

### Performance tips

1.  Have a lot of fast CPU cores and enough RAM to keep them all busy.
    (Minimum recommended is 4-8 fast cores and 16-32 GB of RAM)
2.  Reduce file system overhead by excluding build directories from
    antivirus and indexing software.
3.  Store the build tree on a fast disk (preferably SSD).
4.  If you are primarily going to be doing debug development builds, you
    should use the component build:
    - for [GYP](https://www.chromium.org/developers/gyp-environment-variables)
      set `GYP_DEFINES=component=shared_library`
    - for [GN](https://www.chromium.org/developers/gn-build-configuration),
      set the build arg `is_component_build = true`.
    This will generate many DLLs and enable incremental linking, which makes
    linking **much** faster in Debug.

Still, expect build times of 30 minutes to 2 hours when everything has to
be recompiled.
