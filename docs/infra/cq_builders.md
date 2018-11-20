# List of CQ builders

This page is auto generated using the script
//infra/config/branch/cq_config_presubmit.py. Do not manually edit.

[TOC]

Each builder name links to that builder on Milo. The "Backing builders" links
point to the file used to determine which configurations a builder should copy
when running. These links might 404 or error; they are hard-coded right now,
using common assumptions about how builders are configured.

## Required builders

These builders must pass before a CL may land.

* [android-binary-size](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android-binary-size) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+android-binary-size)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android-binary-size))

* [android-kitkat-arm-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android-kitkat-arm-rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+android-kitkat-arm-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android-kitkat-arm-rel))

* [android-marshmallow-arm64-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android-marshmallow-arm64-rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+android-marshmallow-arm64-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android-marshmallow-arm64-rel))

* [android_arm64_dbg_recipe](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_arm64_dbg_recipe) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+android_arm64_dbg_recipe)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_arm64_dbg_recipe))

* [android_clang_dbg_recipe](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_clang_dbg_recipe) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+android_clang_dbg_recipe)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_clang_dbg_recipe))

* [android_compile_dbg](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_compile_dbg) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+android_compile_dbg)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_compile_dbg))

* [android_cronet](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_cronet) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+android_cronet)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_cronet))

* [cast_shell_android](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/cast_shell_android) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+cast_shell_android)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+cast_shell_android))

* [cast_shell_linux](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/cast_shell_linux) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+cast_shell_linux)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+cast_shell_linux))

* [chromeos-amd64-generic-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/chromeos-amd64-generic-rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+chromeos-amd64-generic-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+chromeos-amd64-generic-rel))

* [chromeos-daisy-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/chromeos-daisy-rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+chromeos-daisy-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+chromeos-daisy-rel))

* [chromium_presubmit](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/chromium_presubmit) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+chromium_presubmit)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+chromium_presubmit))

* [fuchsia_arm64](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/fuchsia_arm64) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+fuchsia_arm64)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+fuchsia_arm64))

* [fuchsia_x64](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/fuchsia_x64) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+fuchsia_x64)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+fuchsia_x64))

* [ios-simulator](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/ios-simulator) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+ios-simulator)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+ios-simulator))

* [linux-chromeos-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-chromeos-rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux-chromeos-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-chromeos-rel))

* [linux-jumbo-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-jumbo-rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux-jumbo-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-jumbo-rel))

* [linux-libfuzzer-asan-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-libfuzzer-asan-rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux-libfuzzer-asan-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-libfuzzer-asan-rel))

* [linux-ozone-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-ozone-rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux-ozone-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-ozone-rel))

* [linux_chromium_asan_rel_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_chromium_asan_rel_ng) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux_chromium_asan_rel_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_chromium_asan_rel_ng))

* [linux_chromium_compile_dbg_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_chromium_compile_dbg_ng) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux_chromium_compile_dbg_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_chromium_compile_dbg_ng))

* [linux_chromium_headless_rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_chromium_headless_rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux_chromium_headless_rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_chromium_headless_rel))

* [linux_chromium_rel_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_chromium_rel_ng) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux_chromium_rel_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_chromium_rel_ng))

* [linux_chromium_tsan_rel_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_chromium_tsan_rel_ng) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux_chromium_tsan_rel_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_chromium_tsan_rel_ng))

* [mac_chromium_compile_dbg_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/mac_chromium_compile_dbg_ng) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+mac_chromium_compile_dbg_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+mac_chromium_compile_dbg_ng))

* [mac_chromium_rel_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/mac_chromium_rel_ng) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+mac_chromium_rel_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+mac_chromium_rel_ng))

* [win10_chromium_x64_rel_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win10_chromium_x64_rel_ng) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+win10_chromium_x64_rel_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+win10_chromium_x64_rel_ng))

* [win7_chromium_rel_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win7_chromium_rel_ng) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+win7_chromium_rel_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+win7_chromium_rel_ng))

* [win_chromium_compile_dbg_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win_chromium_compile_dbg_ng) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+win_chromium_compile_dbg_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+win_chromium_compile_dbg_ng))


## Optional builders

These builders optionally run, depending on the files in a
CL. For example, a CL which touches `//gpu/BUILD.gn` would trigger the builder
`android_optional_gpu_tests_rel`, due to the `path_regexp` values for that
builder.

* [android_optional_gpu_tests_rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_optional_gpu_tests_rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+android_optional_gpu_tests_rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_optional_gpu_tests_rel))

  Path regular expressions:
    * [`//cc/.+`](https://cs.chromium.org/chromium/src/cc/)
    * [`//chrome/browser/vr/.+`](https://cs.chromium.org/chromium/src/chrome/browser/vr/)
    * [`//components/viz/.+`](https://cs.chromium.org/chromium/src/components/viz/)
    * [`//content/test/gpu/.+`](https://cs.chromium.org/chromium/src/content/test/gpu/)
    * [`//gpu/.+`](https://cs.chromium.org/chromium/src/gpu/)
    * [`//media/(audio|filters|gpu)/.+`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:media/(audio|filters|gpu)/)
    * [`//services/viz/.+`](https://cs.chromium.org/chromium/src/services/viz/)
    * [`//testing/trigger_scripts/.+`](https://cs.chromium.org/chromium/src/testing/trigger_scripts/)
    * [`//third_party/blink/renderer/modules/webgl/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/webgl/)
    * [`//ui/gl/.+`](https://cs.chromium.org/chromium/src/ui/gl/)

* [closure_compilation](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/closure_compilation) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+closure_compilation)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+closure_compilation))

  Path regular expressions:
    * [`//components/offline_pages/resources/.+`](https://cs.chromium.org/chromium/src/components/offline_pages/resources/)
    * [`//third_party/closure_compiler/.+`](https://cs.chromium.org/chromium/src/third_party/closure_compiler/)
    * [`//third_party/polymer/.+`](https://cs.chromium.org/chromium/src/third_party/polymer/)

* [ios-simulator-cronet](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/ios-simulator-cronet) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+ios-simulator-cronet)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+ios-simulator-cronet))

  Path regular expressions:
    * [`//components/cronet/.+`](https://cs.chromium.org/chromium/src/components/cronet/)
    * [`//components/grpc_support/.+`](https://cs.chromium.org/chromium/src/components/grpc_support/)
    * [`//ios/.+`](https://cs.chromium.org/chromium/src/ios/)

* [ios-simulator-full-configs](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/ios-simulator-full-configs) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+ios-simulator-full-configs)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+ios-simulator-full-configs))

  Path regular expressions:
    * [`//ios/.+`](https://cs.chromium.org/chromium/src/ios/)

* [linux-blink-gen-property-trees](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-blink-gen-property-trees) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux-blink-gen-property-trees)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-blink-gen-property-trees))

  Path regular expressions:
    * [`//third_party/WebKit/LayoutTests/FlagExpectations/enable-blink-gen-property-trees`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/WebKit/LayoutTests/FlagExpectations/enable-blink-gen-property-trees)
    * [`//third_party/WebKit/LayoutTests/flag-specific/enable-blink-gen-property-trees/.+`](https://cs.chromium.org/chromium/src/third_party/WebKit/LayoutTests/flag-specific/enable-blink-gen-property-trees/)

* [linux-blink-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-blink-rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux-blink-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-blink-rel))

  Path regular expressions:
    * [`//cc/.+`](https://cs.chromium.org/chromium/src/cc/)
    * [`//third_party/WebKit/LayoutTests/FlagExpectations/(enable-slimming-paint-v2|enable-blink-gen-property-trees)`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/WebKit/LayoutTests/FlagExpectations/(enable-slimming-paint-v2|enable-blink-gen-property-trees))
    * [`//third_party/WebKit/LayoutTests/flag-specific/(enable-slimming-paint-v2|enable-blink-gen-property-trees)/.+`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/WebKit/LayoutTests/flag-specific/(enable-slimming-paint-v2|enable-blink-gen-property-trees)/)
    * [`//third_party/blink/renderer/core/layout/compositing/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/layout/compositing/)
    * [`//third_party/blink/renderer/core/(svg|paint)/.+`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/blink/renderer/core/(svg|paint)/)
    * [`//third_party/blink/renderer/platform/graphics/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/graphics/)

* [linux_chromium_dbg_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_chromium_dbg_ng) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux_chromium_dbg_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_chromium_dbg_ng))

  Path regular expressions:
    * [`//build/.*check_gn_headers.*`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:build/.*check_gn_headers.*)

* [linux_layout_tests_layout_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_layout_tests_layout_ng) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux_layout_tests_layout_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_layout_tests_layout_ng))

  Path regular expressions:
    * [`//third_party/Webkit/LayoutTests/FlagExpectations/enable-blink-features=LayoutNG`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/Webkit/LayoutTests/FlagExpectations/enable-blink-features=LayoutNG)
    * [`//third_party/Webkit/LayoutTests/flag-specific/enable-blink-features=LayoutNG/.+`](https://cs.chromium.org/chromium/src/third_party/Webkit/LayoutTests/flag-specific/enable-blink-features=LayoutNG/)
    * [`//third_party/blink/renderer/core/editing/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/editing/)
    * [`//third_party/blink/renderer/core/(layout|paint)/ng/.+`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/blink/renderer/core/(layout|paint)/ng/)
    * [`//third_party/blink/renderer/platform/fonts/shaping/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/fonts/shaping/)

* [linux_layout_tests_slimming_paint_v2](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_layout_tests_slimming_paint_v2) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux_layout_tests_slimming_paint_v2)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_layout_tests_slimming_paint_v2))

  Path regular expressions:
    * [`//third_party/WebKit/LayoutTests/FlagExpectations/(enable-slimming-paint-v2|enable-blink-gen-property-trees)`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/WebKit/LayoutTests/FlagExpectations/(enable-slimming-paint-v2|enable-blink-gen-property-trees))
    * [`//third_party/WebKit/LayoutTests/flag-specific/(enable-slimming-paint-v2|enable-blink-gen-property-trees)/.+`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/WebKit/LayoutTests/flag-specific/(enable-slimming-paint-v2|enable-blink-gen-property-trees)/)
    * [`//third_party/blink/renderer/core/layout/compositing/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/layout/compositing/)
    * [`//third_party/blink/renderer/core/(svg|paint)/.+`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/blink/renderer/core/(svg|paint)/)
    * [`//third_party/blink/renderer/platform/graphics/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/graphics/)

* [linux_mojo](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_mojo) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux_mojo)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_mojo))

  Path regular expressions:
    * [`//services/network/.+`](https://cs.chromium.org/chromium/src/services/network/)
    * [`//testing/buildbot/filters/mojo\\.fyi\\.network_.*`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:testing/buildbot/filters/mojo\\.fyi\\.network_.*)
    * [`//third_party/WebKit/LayoutTests/FlagExpectations/enable-features=NetworkService`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/WebKit/LayoutTests/FlagExpectations/enable-features=NetworkService)

* [linux_optional_gpu_tests_rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_optional_gpu_tests_rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux_optional_gpu_tests_rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_optional_gpu_tests_rel))

  Path regular expressions:
    * [`//chrome/browser/vr/.+`](https://cs.chromium.org/chromium/src/chrome/browser/vr/)
    * [`//content/test/gpu/.+`](https://cs.chromium.org/chromium/src/content/test/gpu/)
    * [`//gpu/.+`](https://cs.chromium.org/chromium/src/gpu/)
    * [`//media/(audio|filters|gpu)/.+`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:media/(audio|filters|gpu)/)
    * [`//testing/trigger_scripts/.+`](https://cs.chromium.org/chromium/src/testing/trigger_scripts/)
    * [`//third_party/blink/renderer/modules/webgl/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/webgl/)
    * [`//ui/gl/.+`](https://cs.chromium.org/chromium/src/ui/gl/)

* [linux_vr](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_vr) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux_vr)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_vr))

  Path regular expressions:
    * [`//chrome/browser/vr/.+`](https://cs.chromium.org/chromium/src/chrome/browser/vr/)

* [mac_optional_gpu_tests_rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/mac_optional_gpu_tests_rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+mac_optional_gpu_tests_rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+mac_optional_gpu_tests_rel))

  Path regular expressions:
    * [`//chrome/browser/vr/.+`](https://cs.chromium.org/chromium/src/chrome/browser/vr/)
    * [`//content/test/gpu/.+`](https://cs.chromium.org/chromium/src/content/test/gpu/)
    * [`//gpu/.+`](https://cs.chromium.org/chromium/src/gpu/)
    * [`//media/(audio|filters|gpu)/.+`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:media/(audio|filters|gpu)/)
    * [`//services/shape_detection/.+`](https://cs.chromium.org/chromium/src/services/shape_detection/)
    * [`//testing/trigger_scripts/.+`](https://cs.chromium.org/chromium/src/testing/trigger_scripts/)
    * [`//third_party/blink/renderer/modules/webgl/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/webgl/)
    * [`//ui/gl/.+`](https://cs.chromium.org/chromium/src/ui/gl/)

* [win_optional_gpu_tests_rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win_optional_gpu_tests_rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+win_optional_gpu_tests_rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+win_optional_gpu_tests_rel))

  Path regular expressions:
    * [`//chrome/browser/vr/.+`](https://cs.chromium.org/chromium/src/chrome/browser/vr/)
    * [`//content/test/gpu/.+`](https://cs.chromium.org/chromium/src/content/test/gpu/)
    * [`//device/vr/.+`](https://cs.chromium.org/chromium/src/device/vr/)
    * [`//gpu/.+`](https://cs.chromium.org/chromium/src/gpu/)
    * [`//media/(audio|filters|gpu)/.+`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:media/(audio|filters|gpu)/)
    * [`//testing/trigger_scripts/.+`](https://cs.chromium.org/chromium/src/testing/trigger_scripts/)
    * [`//third_party/blink/renderer/modules/vr/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/vr/)
    * [`//third_party/blink/renderer/modules/webgl/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/webgl/)
    * [`//third_party/blink/renderer/modules/xr/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/xr/)
    * [`//third_party/blink/renderer/platform/graphics/gpu/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/graphics/gpu/)
    * [`//ui/gl/.+`](https://cs.chromium.org/chromium/src/ui/gl/)

* [android_compile_x64_dbg](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_compile_x64_dbg) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+android_compile_x64_dbg)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_compile_x64_dbg))

  Path regular expressions:
    * [`//sandbox/linux/(bpd_dsl|seccomp-bpf|secomp-bpf-helpers|system_headers|tests)/.+`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:sandbox/linux/(bpd_dsl|seccomp-bpf|secomp-bpf-helpers|system_headers|tests)/)

* [android_compile_x86_dbg](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_compile_x86_dbg) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+android_compile_x86_dbg)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_compile_x86_dbg))

  Path regular expressions:
    * [`//sandbox/linux/(bpd_dsl|seccomp-bpf|secomp-bpf-helpers|system_headers|tests)/.+`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:sandbox/linux/(bpd_dsl|seccomp-bpf|secomp-bpf-helpers|system_headers|tests)/)

* [android_cronet_tester](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_cronet_tester) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+android_cronet_tester)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_cronet_tester))

  Path regular expressions:
    * [`//components/cronet/.+`](https://cs.chromium.org/chromium/src/components/cronet/)
    * [`//components/grpc_support/.+`](https://cs.chromium.org/chromium/src/components/grpc_support/)


## Experimental builders

These builders are run on some percentage of builds. Their results are ignored
by CQ. These are often used to test new configurations before they are added
as required builders.

* [ios-device](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/ios-device) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+ios-device)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+ios-device))

  https://crbug.com/739556; make this non-experimental ASAP.

  * Experimental percentage: 10

* [ios-device-xcode-clang](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/ios-device-xcode-clang) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+ios-device-xcode-clang)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+ios-device-xcode-clang))

  https://crbug.com/739556

  * Experimental percentage: 10

* [ios-simulator-xcode-clang](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/ios-simulator-xcode-clang) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+ios-simulator-xcode-clang)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+ios-simulator-xcode-clang))

  https://crbug.com/739556

  * Experimental percentage: 10

* [linux-chromeos-compile-dbg](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-chromeos-compile-dbg) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux-chromeos-compile-dbg)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-chromeos-compile-dbg))

  * Experimental percentage: 50

* [linux-dcheck-off-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-dcheck-off-rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux-dcheck-off-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-dcheck-off-rel))

  https://crbug.com/833482

  * Experimental percentage: 10

* [linux-goma-rbe-staging-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-goma-rbe-staging-rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+linux-goma-rbe-staging-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-goma-rbe-staging-rel))

  https://crbug.com/855319

  * Experimental percentage: 20

* [win-libfuzzer-asan-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win-libfuzzer-asan-rel) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+win-libfuzzer-asan-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+win-libfuzzer-asan-rel))

  * Experimental percentage: 100

* [win7_chromium_rel_loc_exp](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win7_chromium_rel_loc_exp) ([`cq.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:cq.cfg+win7_chromium_rel_loc_exp)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+win7_chromium_rel_loc_exp))

  * Experimental percentage: 20

