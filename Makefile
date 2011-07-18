CXX      := g++
CXXFLAGS := -W -Wall -g
LIBS     := libmkvparser.a libmkvmuxer.a
PARSEOBJ := mkvparser.o mkvreader.o
MUXEROBJ := mkvmuxer.o mkvmuxerutil.o mkvwriter.o
OBJECTS1 := $(PARSEOBJ) sample.o
OBJECTS2 := $(PARSEOBJ) $(MUXEROBJ) sample_muxer/sample_muxer.o
INCLUDES := -I.
EXES     := samplemuxer sample

all: $(EXES)

sample: sample.o libmkvparser.a
	$(CXX) $^ -o $@

samplemuxer: sample_muxer/sample_muxer.o $(LIBS)
	$(CXX) $^ -o $@

libmkvparser.a: $(PARSEOBJ)
	$(AR) rcs $@ $^

libmkvmuxer.a: $(MUXEROBJ)
	$(AR) rcs $@ $^

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

clean:
	$(RM) -f $(OBJECTS1) $(OBJECTS2) $(LIBS) $(EXES) Makefile.bak
