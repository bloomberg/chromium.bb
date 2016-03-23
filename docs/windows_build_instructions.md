# Windows Build Instructions

## Setting up Windows

You must set your Windows system locale to English, or else you may get
build errors about "The file contains a character that cannot be
represented in the current code page."

### Setting up the environment for Visual Studio

You must build with Visual Studio 2013 Update 4 or Visual Studio 2015
Update 1, no other versions are supported.

You must have Windows 7 x64 or later. x86 OSs are unsupported.

1.  Get
[depot\_tools](http://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up).
2.  Follow the appropriate path below:

### Open source contributors

For building with Visual Studio 2015 (default compiler as of March 10,
2016):

> Install Visual Studio 2015 Update 1 or later - Community Edition
> should work if its license is appropriate for you. Be sure to select
> Custom install and select VC++ (which selects three sub-categories
> including MFC) and, under Universal Windows App Development Tools,
> select Tools (1.2) and Windows 10 SDK (10.0.10586). You must have the
> 10586 SDK installed or else you will hit compile errors such as
> redefined macros.

For building with Visual Studio 2013 (no longer default as of March 10,
2016, and not recommended - requires setting GYP\_MSVS\_VERSION=2013):

> Install [Visual Studio 2013
> Community](http://www.visualstudio.com/products/visual-studio-community-vs)
> or [Visual Studio 2013
> Professional](http://www.visualstudio.com/products/visual-studio-professional-with-msdn-vs)
> depending on which license is appropriate for you. You can deselect
> the default options if you want, but you must make sure to install
> "Microsoft Foundation Classes for C++".
> \
> You should also install the [Windows 10
> SDK](https://dev.windows.com/en-us/downloads/windows-10-sdk) to the
> default install location. You must have SDK version 10.0.10586 or
> greater installed.

Run `set DEPOT\_TOOLS\_WIN\_TOOLCHAIN=0`, or set that variable in your
global environment.

Visual Studio Express 2013 is **not** supported and will not be able to
build Chromium.

Compilation is done through ninja, **not** Visual Studio.

### Google employees

Run: `download\_from\_google\_storage --config` and follow the
authentication instructions.**Note that you must authenticate with your
@google.com credentials**, not @chromium.org. Enter "0" if asked for a
project-id.

Once you've done this, the toolchain will be installed automatically for
you in Step 3, below (near the end of the step).

The toolchain will be in depot\_tools\\win\_toolchain, and windbg can be
found in
depot\_tools\\win\_toolchain\\vs2013\_files\\win8sdk\\Debuggers.

If you want the IDE for debugging and editing, you will need to install
it separately, but this is optional and not needed to build Chromium.

## Getting the Code

Follow the steps to [check out the
code](https://www.chromium.org/developers/how-tos/get-the-code) (largely
"fetch chromium").

## Building

Build the target you are interested in.

```shell
ninja -C out\\Debug chrome
```

Alternative (Graphical user interface): Open a generated .sln
file such as all.sln, right-click the chrome project and select build.
This will invoke the real step 4 above. Do not build the whole solution
since that conflicts with ninja's build management and everything will
explode.
Substitute the build directory given to -C with out\\Debug\_x64 for
[64-bit
builds](https://www.chromium.org/developers/design-documents/64-bit-support)
in GYP, or whatever build directory you have configured if using GN.

### Performance tips

1.  Have many and fast CPU cores and enough RAM to keep them all busy.
    (Minimum recommended is 4-8 fast cores and 16-32 GB of RAM)
2.  Reduce file system overhead by excluding build directories from
    antivirus and indexing software.
3.  Store the build tree on a fast disk (preferably SSD).
4.  If you are primarily going to be doing debug development builds, you
    use the component build (in
    [GYP](https://www.chromium.org/developers/gyp-environment-variables)
    do set GYP\_DEFINES=component=shared\_library, in
    [GN](https://www.chromium.org/developers/gn-build-configuration),
    set the build arg is\_component\_build = true). This will generate
    many DLLs and enable incremental linking, which makes linking
    *much*faster in Debug.

Still expect build times of 30 minutes to 2 hours when everything has to
be recompiled.
