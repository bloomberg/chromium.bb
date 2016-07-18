# Android Studio

Android Studio integration works by generating .gradle files from our BUILD.gn files.

[TOC]

## Usage

```shell
build/android/gradle/generate_gradle.py --output-directory out-gn/Debug --target //chrome/android:chrome_public_apk
```

This creates a project at `out-gn/Debug/gradle`. To create elsewhere: `--project-dir foo`

## Status (as of July 14, 2016)

### What currently works

 - Basic Java editing and compiling

### Roadmap / what's not yet implemented ([crbug](https://bugs.chromium.org/p/chromium/issues/detail?id=620034))

 - Test targets (although they *somewhat* work via `--target=//chrome/android:chrome_public_test_apk__apk`)
 - Make gradle aware of resources and assets
 - Make gradle aware of native code via pointing it at the location of our .so
 - Add a mode in which gradle is responsible for generating `R.java`
 - Add support for native code editing

### What's odd about our integration

 - We disable generation of `R.java`, `BuildConfig.java`, `AndroidManifest.java`
 - Generated .java files (.srcjars) are extracted to the project directory upon project creation. They are not re-extracted unless you manually run `generate_gradle.py` again

## Android Studio Tips

 - Configuration instructions can be found [here](http://tools.android.com/tech-docs/configuration). Some suggestions:
   - Launch it with more RAM: `STUDIO_VM_OPTIONS=-Xmx2048m /opt/android-studio-stable/bin/studio-launcher.sh`
 - Setup wizard advice:
   - Choose "Standard", it then fails (at least for me) from "SDK tools directory is missing". Oh well...
   - Choose "Import" and select your generated project directory
   - Choose "OK" to set up gradle wrapper
 - If you ever need to reset it: `rm -r ~/.AndroidStudio*/`

