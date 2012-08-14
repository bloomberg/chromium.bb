CXX       := g++
CXXFLAGS  := -W -Wall -g
LIBWEBMA  := libwebm.a
LIBWEBMSO := libwebm.so
WEBMOBJS  := mkvparser.o mkvreader.o mkvmuxer.o mkvmuxerutil.o mkvwriter.o
OBJSA     := $(WEBMOBJS:.o=_a.o)
OBJSSO    := $(WEBMOBJS:.o=_so.o)
OBJECTS1  := sample.o
OBJECTS2  := sample_muxer.o
OBJECTS3  := dumpvtt.o vttreader.o webvttparser.o
OBJECTS4  := vttreader.o webvttparser.o sample_muxer_metadata.o
INCLUDES  := -I.
EXES      := samplemuxer sample dumpvtt

all: $(EXES)

sample: sample.o $(LIBWEBMA)
	$(CXX) $^ -o $@

samplemuxer: sample_muxer.o $(LIBWEBMA) $(OBJECTS4)
	$(CXX) $^ -o $@

dumpvtt: $(OBJECTS3)
	$(CXX) $^ -o $@

shared: $(LIBWEBMSO)

libwebm.a: $(OBJSA)
	$(AR) rcs $@ $^

libwebm.so: $(OBJSSO)
	$(CXX) $(CXXFLAGS) -shared $(OBJSSO) -o $(LIBWEBMSO)

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

%_a.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

%_so.o: %.cpp
	$(CXX) -c $(CXXFLAGS) -fPIC $(INCLUDES) $< -o $@

clean:
	$(RM) -f $(OBJECTS1) $(OBJECTS2) $(OBJECTS3) $(OBJECTS4) $(OBJSA) $(OBJSSO) $(LIBWEBMA) $(LIBWEBMSO) $(EXES) Makefile.bak
