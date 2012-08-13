###################################################
#
# Makefile for libjson.a
#
###################################################

C_SRCS=
C_OPTIONS=

CPP_SRCS= \
	./src/lib_json/json_reader.cpp \
	./src/lib_json/json_value.cpp \
	./src/lib_json/json_writer.cpp
CPP_OPTIONS=

INCLUDE= ./include

LIBRARY=libjson.a

include ../../jumper/source/common.mak

