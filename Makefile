# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CHROME=google-chrome

# Include all resources of the Camera App to be copied to the target package,
# but without the manifest files.
SRC_RESOURCES= \
	src/_locales/ar/messages.json \
	src/_locales/bg/messages.json \
	src/_locales/bn/messages.json \
	src/_locales/ca/messages.json \
	src/_locales/cs/messages.json \
	src/_locales/da/messages.json \
	src/_locales/de/messages.json \
	src/_locales/el/messages.json \
	src/_locales/en/messages.json \
	src/_locales/en_GB/messages.json \
	src/_locales/es/messages.json \
	src/_locales/es_419/messages.json \
	src/_locales/et/messages.json \
	src/_locales/fa/messages.json \
	src/_locales/fi/messages.json \
	src/_locales/fil/messages.json \
	src/_locales/fr/messages.json \
	src/_locales/gu/messages.json \
	src/_locales/he/messages.json \
	src/_locales/hi/messages.json \
	src/_locales/hr/messages.json \
	src/_locales/hu/messages.json \
	src/_locales/id/messages.json \
	src/_locales/it/messages.json \
	src/_locales/ja/messages.json \
	src/_locales/kn/messages.json \
	src/_locales/ko/messages.json \
	src/_locales/lt/messages.json \
	src/_locales/lv/messages.json \
	src/_locales/ml/messages.json \
	src/_locales/mr/messages.json \
	src/_locales/ms/messages.json \
	src/_locales/nl/messages.json \
	src/_locales/no/messages.json \
	src/_locales/pl/messages.json \
	src/_locales/pt_BR/messages.json \
	src/_locales/pt_PT/messages.json \
	src/_locales/ro/messages.json \
	src/_locales/ru/messages.json \
	src/_locales/sk/messages.json \
	src/_locales/sl/messages.json \
	src/_locales/sr/messages.json \
	src/_locales/sv/messages.json \
	src/_locales/ta/messages.json \
	src/_locales/te/messages.json \
	src/_locales/th/messages.json \
	src/_locales/tr/messages.json \
	src/_locales/uk/messages.json \
	src/_locales/vi/messages.json \
	src/_locales/zh_CN/messages.json \
	src/_locales/zh_TW/messages.json \
	src/css/main.css \
	src/images/2x/album_video_overlay.png \
	src/images/2x/browser_button_export.png \
	src/images/2x/browser_button_print.png \
	src/images/2x/camera_button_album.png \
	src/images/2x/camera_button_filters.png \
	src/images/2x/camera_button_mirror.png \
	src/images/2x/camera_button_multi.png \
	src/images/2x/camera_button_picture.png \
        src/images/2x/camera_button_record.png \
	src/images/2x/camera_button_timer.png \
	src/images/2x/camera_button_toggle.png \
	src/images/2x/camera_button_video.png \
	src/images/2x/gallery_button_back.png \
	src/images/2x/gallery_button_delete.png \
        src/images/2x/dialog_button_close.png \
	src/images/album_video_overlay.png \
	src/images/browser_button_export.png \
	src/images/browser_button_print.png \
	src/images/camera_app_icons_128.png \
	src/images/camera_app_icons_256.png \
	src/images/camera_app_icons_32.png \
	src/images/camera_app_icons_48.png \
	src/images/camera_app_icons_64.png \
	src/images/camera_app_icons_96.png \
	src/images/camera_app_icons_favicon_16.png \
	src/images/camera_app_icons_favicon_32.png \
	src/images/camera_button_album.png \
	src/images/camera_button_filters.png \
	src/images/camera_button_mirror.png \
	src/images/camera_button_multi.png \
	src/images/camera_button_picture.png \
        src/images/camera_button_record.png \
	src/images/camera_button_timer.png \
	src/images/camera_button_toggle.png \
	src/images/camera_button_video.png \
	src/images/gallery_button_back.png \
	src/images/gallery_button_delete.png \
        src/images/dialog_button_close.png \
	src/images/no_camera.svg \
	src/images/spinner.svg \
	src/js/background.js \
	src/js/main.js \
	src/js/models/gallery.js \
	src/js/processor.js \
	src/js/router.js \
	src/js/scrollbar.js \
	src/js/test.js \
	src/js/test_cases.js \
	src/js/util.js \
	src/js/view.js \
	src/js/views/album.js \
	src/js/views/browser.js \
	src/js/views/camera.js \
	src/js/views/dialog.js \
	src/js/views/gallery_base.js \
        src/sounds/record_end.ogg \
        src/sounds/record_start.ogg \
	src/sounds/shutter.ogg \
	src/sounds/tick.ogg \
	src/views/main.html \

# Path for the Camera resources. Relative, with a trailing slash.
SRC_PATH=src/

# Manifest file for the camera.crx package.
SRC_MANIFEST=src/manifest.json

# Manifest file for the tests.crx package.
SRC_TESTS_MANIFEST=src/manifest-tests.json

# Resources of the third party glfx library. They will be built, and the target
# glfx.js with LICENSE will be copied to the target package.
GLFX_RESOURCES= \
	third_party/glfx/LICENSE \
	third_party/glfx/build.py \
	third_party/glfx/src/OES_texture_float_linear-polyfill.js \
	third_party/glfx/src/core/canvas.js \
	third_party/glfx/src/core/matrix.js \
	third_party/glfx/src/core/shader.js \
	third_party/glfx/src/core/spline.js \
	third_party/glfx/src/core/texture.js \
	third_party/glfx/src/filters/adjust/brightnesscontrast.js \
	third_party/glfx/src/filters/adjust/curves.js \
	third_party/glfx/src/filters/adjust/denoise.js \
	third_party/glfx/src/filters/adjust/huesaturation.js \
	third_party/glfx/src/filters/adjust/noise.js \
	third_party/glfx/src/filters/adjust/sepia.js \
	third_party/glfx/src/filters/adjust/unsharpmask.js \
	third_party/glfx/src/filters/adjust/vibrance.js \
	third_party/glfx/src/filters/adjust/vignette.js \
	third_party/glfx/src/filters/blur/lensblur.js \
	third_party/glfx/src/filters/blur/tiltshift.js \
	third_party/glfx/src/filters/blur/triangleblur.js \
	third_party/glfx/src/filters/blur/zoomblur.js \
	third_party/glfx/src/filters/common.js \
	third_party/glfx/src/filters/fun/colorhalftone.js \
	third_party/glfx/src/filters/fun/dotscreen.js \
	third_party/glfx/src/filters/fun/edgework.js \
	third_party/glfx/src/filters/fun/ghost.js \
	third_party/glfx/src/filters/fun/hexagonalpixelate.js \
	third_party/glfx/src/filters/fun/ink.js \
	third_party/glfx/src/filters/fun/modern.js \
	third_party/glfx/src/filters/fun/photolab.js \
	third_party/glfx/src/filters/warp/bulgepinch.js \
	third_party/glfx/src/filters/warp/matrixwarp.js \
	third_party/glfx/src/filters/warp/perspective.js \
	third_party/glfx/src/filters/warp/swirl.js \
	third_party/glfx/www/build.py \

# Builds camera.crx and tests.crx
all: build/camera.crx build/tests.crx

# Builds the glfx third_party library.
build/third_party/glfx: $(GLFX_RESOURCES)
	mkdir -p build
	cp --parents $(GLFX_RESOURCES) build
	cd build/third_party/glfx && ./build.py

# Copies the built glfx library to the camera.crx build directory.
build/camera/js/third_party/glfx: $(GLFX_RESOURCES) build/third_party/glfx
	mkdir -p build/camera/js/third_party/glfx
	cp build/third_party/glfx/glfx.js build/camera/js/third_party/glfx
	cp build/third_party/glfx/LICENSE build/camera/js/third_party/glfx

# Copies the built library to the tests.crx build directory.
build/tests/js/third_party/glfx: $(GLFX_RESOURCES) build/third_party/glfx
	mkdir -p build/tests/js/third_party/glfx
	cp build/third_party/glfx/glfx.js build/tests/js/third_party/glfx
	cp build/third_party/glfx/LICENSE build/tests/js/third_party/glfx

# Builds the release version.
build/camera: $(SRC_RESOURCES) $(SRC_MANIFEST) \
	  build/camera/js/third_party/glfx
	mkdir -p build/camera
	cd $(SRC_PATH); cp --parents $(patsubst $(SRC_PATH)%, %, \
	  $(SRC_RESOURCES)) ../build/camera
	cp $(SRC_MANIFEST) build/camera/manifest.json

# Builds the camera.crx package.
build/camera.crx: build/camera
	$(CHROME) --pack-extension=build/camera \
	  --pack-extension-key=dev-keys/camera.pem

# Alias for build/camera.crx.
camera: build/camera.crx

# Builds the tests version.
build/tests: $(SRC_RESOURCES) $(SRC_TESTS_MANIFEST) \
	  build/tests/js/third_party/glfx
	mkdir -p build/tests
	cd $(SRC_PATH); cp --parents $(patsubst $(SRC_PATH)%, %, \
	  $(SRC_RESOURCES)) ../build/tests
	cp $(SRC_TESTS_MANIFEST) build/tests/manifest.json

# Builds the tests.crx package.
build/tests.crx: build/tests
	$(CHROME) --pack-extension=build/tests \
	  --pack-extension-key=dev-keys/camera.pem

# Alias for build/tests.crx.
tests: build/tests.crx

# Cleans the workspace from build files files.
clean:
	rm -rf build

# Builds the tests.crx package and performs tests on it.
run-tests: build/tests.crx
	tests/run_all_tests.py
