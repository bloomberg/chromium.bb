# Android Studio

[TOC]

## Usage

Make sure you have followed
[android build instructions](android_build_instructions.md) already.

```shell
build/android/gradle/generate_gradle.py
```

This creates a project at `out/Debug/gradle`. To create elsewhere:

```shell
build/android/gradle/generate_gradle.py --output-directory out/My-Out-Dir --project-dir my-project
```

By default, only common targets are generated. To customize the list of targets
to generate projects for:

```shell
build/android/gradle/generate_gradle.py --target //chrome/android:chrome_public_apk --target //android_webview/test:android_webview_apk
```

For first-time Android Studio users:

* Avoid running the setup wizard.
    * The wizard will force you to download unwanted SDK components to
      `//third_party/android_tools`.
    * To skip it, select "Cancel" when it comes up.

To import the project:

* Use "Import Project", and select the directory containing the generated
  project, by default `out/Debug/gradle`.

You need to re-run `generate_gradle.py` whenever `BUILD.gn` files change.

* After regenerating, Android Studio should prompt you to "Sync". If it
  doesn't, use:
    * Button with two arrows on the right side of the top strip.
    * Help -&gt; Find Action -&gt; "Sync Project with Gradle Files"
    * After `gn clean` you may need to restart Android Studio.

## How it Works

Android Studio integration works by generating `build.gradle` files based on GN
targets. Each `android_apk` and `android_library` target produces a separate
Gradle sub-project.

### Excluded files and .srcjars

Gradle supports source directories but not source files. However, some
directories in Chromium are split amonst multiple GN targets. To accommodate
this, the script detects such targets and creates exclude patterns to exclude
files not in the current target. You may still see them when editing, but they
are excluded in gradle tasks.
***

Most generated .java files in GN are stored as `.srcjars`. Android Studio does
not have support for them, and so the generator script builds and extracts them
all to `extracted-srcjars/` subdirectories for each target that contains them.

*** note
** TLDR:** Always re-generate project files when `.srcjars` change (this
includes `R.java`).
***

## Android Studio Tips

* Configuration instructions can be found
  [here](http://tools.android.com/tech-docs/configuration). One suggestions:
    * Launch it with more RAM:
      `STUDIO_VM_OPTIONS=-Xmx2048m /opt/android-studio-stable/bin/studio-launcher.sh`
* If you ever need to reset it: `rm -r ~/.AndroidStudio*/`
* Import Android style settings:
    * Help -&gt; Find Action -&gt; "Code Style" (settings) -&gt; Java -&gt;
      Manage -&gt; Import
        * Select `tools/android/android_studio/ChromiumStyle.xml`
* Turn on automatic import:
    * Help -&gt; Find Action -&gt; "Auto Import"
        * Tick all the boxes under "Java" and change the dropdown to "All".
* Turn on documentation on mouse hover:
    * Help -&gt; Find Action -&gt; "Show quick documentation on mouse move"
* Turn on line numbers:
    * Help -&gt; Find Action -&gt; "Show line numbers"

### Useful Shortcuts

* `Shift - Shift`: Search to open file or perform IDE action
* `Ctrl + N`: Jump to class
* `Ctrl + Shift + T`: Jump to test
* `Ctrl + Shift + N`: Jump to file
* `Ctrl + F12`: Jump to method
* `Ctrl + G`: Jump to line
* `Shift + F6`: Rename variable
* `Ctrl + Alt + O`: Organize imports
* `Alt + Enter`: Quick Fix (use on underlined errors)

### Building from the Command Line

Gradle builds can be done from the command-line after importing the project
into Android Studio (importing into the IDE causes the Gradle wrapper to be
added). This wrapper can also be used to invoke gradle commands.

    cd $GRADLE_PROJECT_DIR && bash gradlew

The resulting artifacts are not terribly useful. They are missing assets,
resources, native libraries, etc.

* Use a
  [gradle daemon](https://docs.gradle.org/2.14.1/userguide/gradle_daemon.html)
  to speed up builds using the gradlew script:
    * Add the line `org.gradle.daemon=true` to `~/.gradle/gradle.properties`,
      creating it if necessary.

## Status (as of Feb 7th, 2017)

### What works

* Tested with Android Studio v2.2.
* Java editing and gradle compile works.
* Instrumentation tests included as androidTest.
* Symlinks to existing .so files in jniLibs (doesn't generate them).
* Editing resource xml files.

### What doesn't work (yet) ([crbug](https://bugs.chromium.org/p/chromium/issues/detail?id=620034))

* Make gradle aware of assets
* Layout editor
* Add a mode in which gradle is responsible for generating `R.java`
* Add support for native code editing
* Make the "Make Project" button work correctly
