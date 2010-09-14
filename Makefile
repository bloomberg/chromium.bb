include config.mk

subdirs = wayland compositor clients spec data

all : subdirs-all

subdirs-all subdirs-clean subdirs-install:
	for f in $(subdirs); do $(MAKE) -C $$f $(@:subdirs-%=%); done

install : subdirs-install

clean : subdirs-clean

config.mk : config.mk.in
	./config.status
