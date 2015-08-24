#summary Build instructions for Linux
#labels Linux,build



## Overview
Due its complexity, Chromium uses a set of custom tools to check out and build.  Here's an overview of the steps you'll run:
  1. **gclient**.  A checkout involves pulling nearly 100 different SVN repositories of code.  This process is managed with a tool called `gclient`.
  1. **gyp**.  The cross-platform build configuration system is called `gyp`, and on Linux it generates ninja build files.  Running `gyp` is analogous to the `./configure` step seen in most other software.
  1. **ninja**.  The actual build itself uses `ninja`. A prebuilt binary is in depot\_tools and should already be in your path if you followed the steps to check out Chromium.
  1. We don't provide any sort of "install" step.
  1. You may want to [use a chroot](http://code.google.com/p/chromium/wiki/UsingALinuxChroot) to isolate yourself from versioning or packaging conflicts (or to run the layout tests).

## Getting a checkout
  * [Prerequisites](LinuxBuildInstructionsPrerequisites.md): what you need before you build
  * [Get the Code](http://dev.chromium.org/developers/how-tos/get-the-code): check out the source code.

**Note**.  If you are working on Chromium OS and already have sources in `chromiumos/chromium`, you **must** run `chrome_set_ver --runhooks` to set the correct dependencies.  This step is otherwise performed by `gclient` as part of your checkout.

## First Time Build Bootstrap
  * Make sure your dependencies are up to date by running the `install-build-deps.sh` script:
```
.../chromium/src$ build/install-build-deps.sh 
```

  * Before you build, you should also  [install API keys](https://sites.google.com/a/chromium.org/dev/developers/how-tos/api-keys).

## `gyp` (configuring)
After `gclient sync` finishes, it will run `gyp` automatically to generate the ninja build files. For standard chromium builds, this automatic step is sufficient and you can start [compiling](https://code.google.com/p/chromium/wiki/LinuxBuildInstructions#Compilation).

To manually configure `gyp`, run `gclient runhooks` or run `gyp` directly via `build/gyp_chromium`. See [Configuring the Build](https://code.google.com/p/chromium/wiki/CommonBuildTasks#Configuring_the_Build) for detailed `gyp` options.

[GypUserDocumentation](https://code.google.com/p/gyp/wiki/GypUserDocumentation) gives background on `gyp`, but is not necessary if you are just building Chromium.

### Configuring `gyp`
See [Configuring the Build](https://code.google.com/p/chromium/wiki/CommonBuildTasks#Configuring_the_Build) for details; most often you'll be changing the `GYP_DEFINES` options, which is discussed here.

`gyp` supports a minimal amount of build configuration via the `-D` flag.
```
build/gyp_chromium -Dflag1=value1 -Dflag2=value2
```
You can store these in the `GYP_DEFINES` environment variable, separating flags with spaces, as in:
```
 export GYP_DEFINES="flag1=value1 flag2=value2"
```
After changing your `GYP_DEFINES` you need to rerun `gyp`, either implicitly via `gclient sync` (which also syncs) or `gclient runhooks` or explicitly via `build/gyp_chromium`.

Note that quotes are not necessary for a single flag, but are useful for clarity; `GYP_DEFINES=flag1=value1` is syntactically valid but can be confusing compared to `GYP_DEFINES="flag1=value1"`.

If you have various flags for various purposes, you may find it more legible to break them up across several lines, taking care to include spaces, such as like this:
```
 export GYP_DEFINES="flag1=value1"\
 " flag2=value2"
```
or like this (allowing comments):
```
 export GYP_DEFINES="flag1=value1" # comment
 GYP_DEFINES+=" flag2=value2" # another comment
```

### Sample configurations
  * **gcc warnings**.  By default we fail to build if there are any compiler warnings.  If you're getting warnings, can't build because of that, but just want to get things done, you can specify `-Dwerror=` to turn that off:
```
# one-off
build/gyp_chromium -Dwerror=
# via variable
export GYP_DEFINES="werror="
build/gyp_chromium
```

  * **ChromeOS**.  `-Dchromeos=1` builds the ChromeOS version of Chrome. This is **not** all of ChromeOS (see [the ChromiumOS](http://www.chromium.org/chromium-os) page for full build instructions), this is just the slightly tweaked version of the browser that runs on that system. Its not designed to be run outside of ChromeOS and some features won't work, but compiling on your Linux desktop can be useful for certain types of development and testing.
```
# one-off
build/gyp_chromium -Dchromeos=1
# via variable
export GYP_DEFINES="chromeos=1"
build/gyp_chromium
```


## Compilation
The weird "`src/`" directory is an artifact of `gclient`. Start with:
```
$ cd src
```

### Build just chrome
```
$ ninja -C out/Debug chrome
```

### Faster builds
See LinuxFasterBuilds

### Build every test
```
$ ninja -C out/Debug
```
The above builds all libraries and tests in all components.  **It will take hours.**

Specifying other target names to restrict the build to just what you're
interested in. To build just the simplest unit test:
```
$ ninja -C out/Debug base_unittests
```

### Clang builds

Information about building with Clang can be found [here](http://code.google.com/p/chromium/wiki/Clang).

### Output

Executables are written in `src/out/Debug/` for Debug builds, and `src/out/Release/` for Release builds.

### Release mode

Pass `-C out/Release` to the ninja invocation:
```
$ ninja -C out/Release chrome
```

### Seeing the commands

If you want to see the actual commands that ninja is invoking, add `-v` to the ninja invocation.
```
$ ninja -v -C out/Debug chrome
```
This is useful if, for example, you are debugging gyp changes, or otherwise need to see what ninja is actually doing.

### Clean builds
All built files are put into the `out/` directory, so to start over with a clean build, just
```
rm -rf out
```
and run `gclient runhooks` or `build\gyp_chromium` again to recreate the ninja build files (which are also stored in `out/`). Or you can run `ninja -C out/Debug -t clean`.

### Linker Crashes
If, during the final link stage:
```
  LINK(target) out/Debug/chrome
```

You get an error like:
```
collect2: ld terminated with signal 6 Aborted terminate called after throwing an instance of 'std::bad_alloc'

collect2: ld terminated with signal 11 [Segmentation fault], core dumped 
```
you are probably running out of memory when linking.  Try one of:
  1. Use the `gold` linker
  1. Build on a 64-bit computer
  1. Build in Release mode (debugging symbols require a lot of memory)
  1. Build as shared libraries (note: this build is for developers only, and may have broken functionality)
Most of these are described on the LinuxFasterBuilds page.

## Advanced Features

  * Building frequently?  See LinuxFasterBuilds.
  * Cross-compiling for ARM? See LinuxChromiumArm.
  * Want to use Eclipse as your IDE?  See LinuxEclipseDev.
  * Built version as Default Browser? See LinuxDevBuildAsDefaultBrowser.

## Next Steps
If you want to contribute to the effort toward a Chromium-based browser for Linux, please check out the [Linux Development page](LinuxDevelopment.md) for more information.