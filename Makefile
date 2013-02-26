# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

OBJDIR = obj
SRC=$(shell readlink -f .)

# Objects for libgestures
SO_OBJECTS=\
	$(OBJDIR)/accel_filter_interpreter.o \
	$(OBJDIR)/activity_log.o \
	$(OBJDIR)/box_filter_interpreter.o \
	$(OBJDIR)/click_wiggle_filter_interpreter.o \
	$(OBJDIR)/filter_interpreter.o \
	$(OBJDIR)/finger_merge_filter_interpreter.o \
	$(OBJDIR)/finger_metrics.o \
	$(OBJDIR)/fling_stop_filter_interpreter.o \
	$(OBJDIR)/gestures.o \
	$(OBJDIR)/iir_filter_interpreter.o \
	$(OBJDIR)/immediate_interpreter.o \
	$(OBJDIR)/integral_gesture_filter_interpreter.o \
	$(OBJDIR)/interpreter.o \
	$(OBJDIR)/logging_filter_interpreter.o \
	$(OBJDIR)/lookahead_filter_interpreter.o \
	$(OBJDIR)/mouse_interpreter.o \
	$(OBJDIR)/non_linearity_filter_interpreter.o \
	$(OBJDIR)/palm_classifying_filter_interpreter.o \
	$(OBJDIR)/prop_registry.o \
	$(OBJDIR)/scaling_filter_interpreter.o \
	$(OBJDIR)/cr48_profile_sensor_filter_interpreter.o \
	$(OBJDIR)/sensor_jump_filter_interpreter.o \
	$(OBJDIR)/split_correcting_filter_interpreter.o \
	$(OBJDIR)/stuck_button_inhibitor_filter_interpreter.o \
	$(OBJDIR)/t5r2_correcting_filter_interpreter.o \
	$(OBJDIR)/trace_marker.o \
	$(OBJDIR)/tracer.o \
	$(OBJDIR)/util.o

# Objects for unittests
TEST_OBJECTS=\
	$(OBJDIR)/accel_filter_interpreter_unittest.o \
	$(OBJDIR)/activity_log_unittest.o \
	$(OBJDIR)/activity_replay_unittest.o \
	$(OBJDIR)/box_filter_interpreter_unittest.o \
	$(OBJDIR)/click_wiggle_filter_interpreter_unittest.o \
	$(OBJDIR)/fling_stop_filter_interpreter_unittest.o \
	$(OBJDIR)/gestures_unittest.o \
	$(OBJDIR)/iir_filter_interpreter_unittest.o \
	$(OBJDIR)/immediate_interpreter_unittest.o \
	$(OBJDIR)/integral_gesture_filter_interpreter_unittest.o \
	$(OBJDIR)/interpreter_unittest.o \
	$(OBJDIR)/list_unittest.o \
	$(OBJDIR)/logging_filter_interpreter_unittest.o \
	$(OBJDIR)/lookahead_filter_interpreter_unittest.o \
	$(OBJDIR)/non_linearity_filter_interpreter_unittest.o \
	$(OBJDIR)/map_unittest.o \
	$(OBJDIR)/mouse_interpreter_unittest.o \
	$(OBJDIR)/palm_classifying_filter_interpreter_unittest.o \
	$(OBJDIR)/prop_registry_unittest.o \
	$(OBJDIR)/scaling_filter_interpreter_unittest.o \
	$(OBJDIR)/cr48_profile_sensor_filter_interpreter_unittest.o \
	$(OBJDIR)/sensor_jump_filter_interpreter_unittest.o \
	$(OBJDIR)/set_unittest.o \
	$(OBJDIR)/split_correcting_filter_interpreter_unittest.o \
	$(OBJDIR)/stuck_button_inhibitor_filter_interpreter_unittest.o \
	$(OBJDIR)/t5r2_correcting_filter_interpreter_unittest.o \
	$(OBJDIR)/trace_marker_unittest.o \
	$(OBJDIR)/tracer_unittest.o \
	$(OBJDIR)/util_unittest.o

# Objects that are neither unittests nor SO objects
MISC_OBJECTS=\
	$(OBJDIR)/activity_replay.o \

TEST_MAIN=\
	$(OBJDIR)/test_main.o

TEST_EXE=test
SONAME=$(OBJDIR)/libgestures.so.0

ALL_OBJECTS=\
	$(TEST_OBJECTS) \
	$(TEST_MAIN) \
	$(SO_OBJECTS) \
	$(MISC_OBJECTS)

ALL_OBJECT_FILES=\
	$(SO_OBJECTS) \
	$(MISC_OBJECTS) \
	$(TEST_OBJECTS) \
	$(TEST_MAIN)

DEPDIR = .deps

DESTDIR = .

CXXFLAGS+=\
	-g \
	-fno-exceptions \
	-fno-strict-aliasing \
	-fPIC \
	-Wall \
	-Wclobbered \
	-Wempty-body \
	-Werror \
	-Wignored-qualifiers \
	-Wmissing-field-initializers \
	-Wmissing-format-attribute \
	-Wmissing-noreturn \
	-Wsign-compare \
	-Wtype-limits \
	-D__STDC_FORMAT_MACROS=1 \
	-D_FILE_OFFSET_BITS=64 \
	-DGESTURES_INTERNAL=1 \
	-I..

LID_TOUCHPAD_HELPER=lid_touchpad_helper
TRY_TOUCHPAD_EXPERIMENT=salsa/try_touch_experiment

# Local compilation needs these flags, esp for code coverage testing
ifeq (g++,$(CXX))
CXXFLAGS+=\
	-O1 \
	-DVCSID="\"1234 TESTVERSION\"" \
	--coverage \
	-ftest-coverage \
	-fprofile-arcs
LINK_FLAGS+=-lgcov
else
CXXFLAGS+=\
	-DXLOGGING
PC_DEPS += pixman-1
endif

PKG_CONFIG ?= pkg-config
BASE_VER ?= 180609
PC_DEPS += libchrome-$(BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CXXFLAGS += $(PC_CFLAGS)
LINK_FLAGS += $(PC_LIBS)

LINK_FLAGS+=\
	-lpthread \
	-lrt

TEST_LINK_FLAGS=\
	-lgcov \
	-lglib-2.0 \
	-lgtest

all: $(SONAME)
	$(MAKE) -C $(LID_TOUCHPAD_HELPER)
	$(MAKE) -C $(TRY_TOUCHPAD_EXPERIMENT)

$(SONAME): $(SO_OBJECTS)
	$(CXX) -shared -o $@ $(SO_OBJECTS) -Wl,-h$(SONAME:$(OBJDIR)/%=%) \
		$(LINK_FLAGS)

$(TEST_EXE): $(ALL_OBJECTS)
	$(CXX) -o $@ $(CXXFLAGS) $(ALL_OBJECTS) $(LINK_FLAGS) $(TEST_LINK_FLAGS)

$(OBJDIR)/%.o : src/%.cc
	mkdir -p $(OBJDIR) $(DEPDIR) || true
	$(CXX) $(CXXFLAGS) -MD -c -o $@ $<
	@mv $(@:$.o=$.d) $(DEPDIR)

LIBDIR = /usr/lib

install: $(SONAME)
	$(MAKE) -C $(LID_TOUCHPAD_HELPER) install
	$(MAKE) -C $(TRY_TOUCHPAD_EXPERIMENT) install
	install -D -m 0755 $(SONAME) \
		$(DESTDIR)$(LIBDIR)/$(SONAME:$(OBJDIR)/%=%)
	ln -s $(SONAME:$(OBJDIR)/%=%) \
		$(DESTDIR)$(LIBDIR)/$(SONAME:$(OBJDIR)/%.0=%)
	install -D -m 0644 \
		include/gestures.h $(DESTDIR)/usr/include/gestures/gestures.h

clean:
	$(MAKE) -C $(LID_TOUCHPAD_HELPER) clean
	$(MAKE) -C $(TRY_TOUCHPAD_EXPERIMENT) clean
	rm -rf $(OBJDIR) $(DEPDIR) $(TEST_EXE) html app.info app.info.orig

setup-in-place:
	mkdir -p $(SRC)/in-place/gestures || true
	ln -sf $(SRC)/include/gestures.h $(SRC)/in-place/gestures/gestures.h
	ln -sf $(SRC)/$(SONAME) $(SRC)/in-place/$(SONAME:$(OBJDIR)/%.0=%)
	ln -sf $(SRC)/$(SONAME) $(SRC)/in-place/$(SONAME:$(OBJDIR)/%=%)

in-place: $(SONAME)

clean-in-place: clean

# Unittest coverage

LCOV_EXE=/usr/bin/lcov

$(LCOV_EXE):
	sudo emerge -DNuv1 dev-util/lcov

cov: $(TEST_EXE) $(LCOV_EXE)
	lcov -d . --zerocounters
	./$(TEST_EXE)
	lcov --directory . --capture --output-file $(OBJDIR)/app.info
	sed -i.orig 's|/obj/src/|/src/|g' $(OBJDIR)/app.info
	sed -i.orig 's|/gestures/gestures/|/gestures/|g' $(OBJDIR)/app.info
	genhtml --no-function-coverage -o html $(OBJDIR)/app.info || \
		genhtml -o html $(OBJDIR)/app.info
	./tools/local_coverage_rate.sh $(OBJDIR)/app.info

.PHONY : clean cov all

-include $(ALL_OBJECT_FILES:$(OBJDIR)/%.o=$(DEPDIR)/%.d)
