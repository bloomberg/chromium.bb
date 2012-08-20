CXX      := g++
CXXFLAGS := -W -Wall -g
LIBWEBM  := libwebm.a
WEBMOBJS := mkvparser.o mkvreader.o mkvmuxer.o mkvmuxerutil.o mkvwriter.o
OBJECTS1 := sample.o
OBJECTS2 := sample_muxer.o
OBJECTS3 := dumpvtt.o vttreader.o webvttparser.o
INCLUDES := -I.
EXES     := samplemuxer sample dumpvtt

all: $(EXES)

sample: sample.o $(LIBWEBM)
	$(CXX) $^ -o $@

samplemuxer: sample_muxer.o $(LIBWEBM)
	$(CXX) $^ -o $@

dumpvtt: $(OBJECTS3)
	$(CXX) $^ -o $@

libwebm.a: $(WEBMOBJS)
	$(AR) rcs $@ $^

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

clean:
	$(RM) -f $(OBJECTS1) $(OBJECTS2) $(OBJECTS3) $(WEBMOBJS) $(LIBWEBM) $(EXES) Makefile.bak
