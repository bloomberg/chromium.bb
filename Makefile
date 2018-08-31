# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CHROME=google-chrome

# Include all resources of the Camera App to be copied to the target package,
# but without the manifest files.
SRC_RESOURCES= \
        src/_locales/am/messages.json \
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
        src/_locales/sw/messages.json \
	src/_locales/ta/messages.json \
	src/_locales/te/messages.json \
	src/_locales/th/messages.json \
	src/_locales/tr/messages.json \
	src/_locales/uk/messages.json \
	src/_locales/vi/messages.json \
	src/_locales/zh_CN/messages.json \
	src/_locales/zh_TW/messages.json \
	src/css/main.css \
        src/images/browser_button_back.svg \
        src/images/browser_button_delete.svg \
        src/images/browser_button_export.svg \
        src/images/browser_button_print.svg \
	src/images/camera_app_icons_128.png \
	src/images/camera_app_icons_48.png \
        src/images/camera_button_grid_off.svg \
        src/images/camera_button_grid_on.svg \
        src/images/camera_button_mic_off.svg \
        src/images/camera_button_mic_on.svg \
        src/images/camera_button_mirror_off.svg \
        src/images/camera_button_mirror_on.svg \
        src/images/camera_button_switch_photo.svg \
        src/images/camera_button_switch_video.svg \
        src/images/camera_button_timer_off.svg \
        src/images/camera_button_timer_on.svg \
        src/images/camera_button_toggle_device.svg \
        src/images/camera_mode_photo.svg \
        src/images/camera_mode_recording.svg \
        src/images/camera_shutter_photo_start.svg \
        src/images/camera_shutter_photo_start_active.svg \
        src/images/camera_shutter_photo_start_hover.svg \
        src/images/camera_shutter_photo_stop.svg \
        src/images/camera_shutter_photo_stop_hover.svg \
        src/images/camera_shutter_recording_start.svg \
        src/images/camera_shutter_recording_start_hover.svg \
        src/images/camera_shutter_recording_stop.svg \
        src/images/camera_shutter_recording_stop_hover.svg \
	src/images/no_camera.svg \
	src/images/spinner.svg \
	src/js/background.js \
	src/js/main.js \
	src/js/models/gallery.js \
	src/js/models/file_system.js \
	src/js/router.js \
	src/js/scrollbar.js \
	src/js/test.js \
	src/js/test_cases.js \
        src/js/tooltip.js \
	src/js/util.js \
	src/js/view.js \
	src/js/views/browser.js \
	src/js/views/camera.js \
        src/js/views/camera/gallerybutton.js \
        src/js/views/camera/layout.js \
        src/js/views/camera/options.js \
        src/js/views/camera/preview.js \
        src/js/views/camera/recordtime.js \
        src/js/views/camera_toast.js \
	src/js/views/dialog.js \
	src/js/views/gallery_base.js \
        src/sounds/record_end.ogg \
        src/sounds/record_start.ogg \
	src/sounds/shutter.ogg \
	src/sounds/tick.ogg \
	src/views/main.html \
        src/LICENSE \

# Path for the Camera resources. Relative, with a trailing slash.
SRC_PATH=src/

# Manifest file for the camera.crx package.
SRC_MANIFEST=src/manifest.json

# Manifest file for the tests.crx package.
# TODO(yuli): Remove tests.crx
SRC_TESTS_MANIFEST=src/manifest-tests.json

# Builds camera.crx
all: build/camera.crx

# Builds the release version.
build/camera: $(SRC_RESOURCES) $(SRC_MANIFEST)
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

# Canary build
canary: build/camera-canary

# Dev build
dev: build/camera-dev.crx

# Builds the canary version.
build/camera-canary: $(SRC_RESOURCES) $(SRC_MANIFEST)
	mkdir -p build/camera-canary
	cd $(SRC_PATH); cp --parents $(patsubst $(SRC_PATH)%, %, \
	  $(SRC_RESOURCES)) ../build/camera-canary
	utils/generate_manifest.py --canary

# Builds the dev version.
build/camera-dev: $(SRC_RESOURCES) $(SRC_MANIFEST)
	mkdir -p build/camera-dev
	cd $(SRC_PATH); cp --parents $(patsubst $(SRC_PATH)%, %, \
	  $(SRC_RESOURCES)) ../build/camera-dev
	utils/generate_manifest.py --dev

# Builds the camera-dev.crx package.
build/camera-dev.crx: build/camera-dev
	$(CHROME) --pack-extension=build/camera-dev \
	  --pack-extension-key=dev-keys/camera.pem

# Builds the tests version.
build/tests: $(SRC_RESOURCES) $(SRC_TESTS_MANIFEST)
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
