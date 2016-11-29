# Clang

[Clang](http://clang.llvm.org/) is a compiler with many desirable features
(outlined on their website).

Chrome can be built with Clang. It is now the default compiler on Mac and Linux
for building Chrome, and it is currently useful for its warning and error
messages on Android and Windows.

See
[the open bugs](http://code.google.com/p/chromium/issues/list?q=label:clang).

[TOC]

## Build instructions

Get clang (happens automatically during `gclient runhooks` on Mac and Linux):

    tools/clang/scripts/update.py

Only needs to be run once per checkout, and clang will be automatically updated
by `gclient runhooks`.

Regenerate the ninja build files with Clang enabled. Again, on Linux and Mac,
Clang is the default compiler.

Run `gn args` and add `is_clang = true` to your args.gn file.

Build: `ninja -C out/gn chrome`

## Reverting to gcc on linux

We don't have bots that test this, but building with gcc4.8+ should still work
on Linux. If your system gcc is new enough, run `gn args` and add `is_clang =
false`.

## Mailing List

http://groups.google.com/a/chromium.org/group/clang/topics

## Using plugins

The
[chromium style plugin](http://dev.chromium.org/developers/coding-style/chromium-style-checker-errors)
is used by default when clang is used.

If you're working on the plugin, you can build it locally like so:

1.  Run `./tools/clang/scripts/update.py --force-local-build --without-android`
    to build the plugin.
1.  Run `ninja -C third_party/llvm-build/Release+Asserts/` to build incrementally.
1.  Build with clang like described above, but, if you use goma, disable it.

To test the FindBadConstructs plugin, run:

    (cd tools/clang/plugins/tests && \
     ./test.py ../../../../third_party/llvm-build/Release+Asserts/bin/clang \
               ../../../../third_party/llvm-build/Release+Asserts/lib/libFindBadConstructs.so)

## Using the clang static analyzer

See [clang_static_analyzer.md](clang_static_analyzer.md).

## Windows

clang can be used as compiler on Windows. Clang uses Visual Studio's linker and
SDK, so you still need to have Visual Studio installed.

Things should compile, and all tests should pass. You can check these bots for
how things are currently looking:
http://build.chromium.org/p/chromium.fyi/console?category=win%20clang

```shell
python tools\clang\scripts\update.py
# run `gn args` and add `is_clang = true` to your args.gn, then...
ninja -C out\gn chrome
```

The `update.py` script only needs to be run once per checkout. Clang will be
kept up to date by `gclient runhooks`.

Current brokenness:

*   To get colored diagnostics, you need to be running
    [ansicon](https://github.com/adoxa/ansicon/releases).
*   Debug info does now work, but support for it is new.  If you see something
    not working right, please file a bug and mark it as blocking the
    [clang/win debug info tracking bug](https://crbug.com/636111).

## Using a custom clang binary

Set `clang_base_path` in your args.gn to the llvm build directory containing
`bin/clang` (i.e. the directory you ran cmake). This [must][1] be an absolute
path. You also need to disable chromium's clang plugin.

Here's an example that also disables debug info and enables the component build
(both not strictly necessary, but they will speed up your build):

```
clang_base_path = getenv("HOME") + "/src/llvm-build"
clang_use_chrome_plugins = false
is_debug = false
symbol_level = 1
is_component_build = true
is_clang = true  # Implicitly set on Mac, Linux, iOS; needed on Win and Android.
```

You can then run `head out/gn/toolchain.ninja` and check that the first to
lines set `cc` and `cxx` to your clang binary. If things look good, run `ninja
-C out/gn` to build.

If your clang revision is very different from the one currently used in chromium

*   Check `tools/clang/scripts/update.py` to find chromium's clang revision
*   You might have to tweak warning flags.

## Using LLD

**Experimental!**

LLD is a relatively new linker from LLVM. The current focus is on Windows and
Linux support, where it can link Chrome approximately twice as fast as gold and
MSVC's link.exe as of this writing. LLD does not yet support generating PDB
files, which makes it hard to debug Chrome while using LLD.

on Windows.
Set `use_lld = true` in args.gn.
