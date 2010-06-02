LIB = libmkvparser.a
OBJECTS = mkvparser.o mkvreader.o sample.o
EXE = sample 
CFLAGS = -W -Wall -g 

$(EXE): $(OBJECTS) 
	$(AR) rcs $(LIB) mkvparser.o 
	$(CXX) $(OBJECTS)  -L./ -lmkvparser -o $(EXE)

mkvparser.o: mkvparser.cpp
	$(CXX) -c $(CFLAGS) mkvparser.cpp -o mkvparser.o

mkvreader.o: mkvreader.cpp
	$(CXX) -c $(CFLAGS) mkvreader.cpp -o mkvreader.o

sample.o: sample.cpp
	$(CXX) -c $(CFLAGS) sample.cpp  -o sample.o

clean:
	rm -rf $(OBJECTS) $(LIB) $(EXE) Makefile.bak
