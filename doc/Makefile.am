DOC_MODULE=fontconfig
DOC2HTML=docbook2html
DOC2TXT=docbook2txt
DOC2MAN=docbook2man

TXT=fontconfig-user.txt fontconfig-devel.txt
HTML=fontconfig-user/index.html fontconfig-devel/index.html
SGML=fontconfig-user.sgml fontconfig-devel.sgml

DOC_FUNCS_FNCS=\
	fcmatrix.fncs \
	fccharset.fncs \
	fcvalue.fncs \
	fcpattern.fncs
	
DOC_FUNCS_SGML=\
	fcmatrix.sgml \
	fccharset.sgml \
	fcvalue.sgml \
	fcpattern.sgml
	
man3_MANS= \
	FcCharSetAddChar.3 \
	FcCharSetCopy.3 \
	FcCharSetCount.3 \
	FcCharSetCreate.3 \
	FcCharSetDestroy.3 \
	FcCharSetEqual.3 \
	FcCharSetFirstPage.3 \
	FcCharSetHasChar.3 \
	FcCharSetIntersect.3 \
	FcCharSetIntersectCount.3 \
	FcCharSetIsSubset.3 \
	FcCharSetNextPage.3 \
	FcCharSetSubtract.3 \
	FcCharSetSubtractCount.3 \
	FcCharSetUnion.3 \
	FcMatrixCopy.3 \
	FcMatrixEqual.3 \
	FcMatrixInit.3 \
	FcMatrixMultiply.3 \
	FcMatrixRotate.3 \
	FcMatrixScale.3 \
	FcMatrixShear.3 \
	FcValueDestroy.3 \
	FcValueSave.3

noinst_PROGRAMS=edit-sgml
edit_sgml_SOURCES=edit-sgml.c

DOCDIR=@DOCDIR@
TARGET_DIR=$(DOCDIR)

SUFFIXES=.fncs .sgml .txt .html

.fncs.sgml:
	$(RM) $@
	./edit-sgml func.sgml < $*.fncs > $*.sgml

.sgml.txt:
	$(RM) $@
	$(DOC2TXT) $*.sgml

EXTRA_DIST = $(TXT) $(HTML) $(SGML) $(man3_MANS)
	
if ENABLE_DOCS
DOCS=$(TXT) $(HTML)
all-local: all-local-docs
clean-local: clean-local-docs
install-data-local: install-local-docs
else
all-local:
clean-local:
install-data-local:
endif

$(man3_MANS): manpage.refs

$(DOCS): $(DOC_FUNCS_SGML)

manpage.refs: fontconfig-devel.sgml $(DOC_FUNCS_SGML)
	$(DOC2MAN) fontconfig-devel.sgml

$(DOC_FUNCS_SGML): edit-sgml func.sgml

all-local-docs: $(DOCS)

clean-local-docs:
	rm -f $(DOCS)

fontconfig-devel/index.html: fontconfig-devel.sgml
	$(RM) -rf fontconfig-devel
	docbook2html -o fontconfig-devel fontconfig-devel.sgml

fontconfig-user/index.html: fontconfig-user.sgml
	$(RM) -rf fontconfig-user
	docbook2html -o fontconfig-user fontconfig-user.sgml

install-local-docs:
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	for i in $(DOCS); do \
	  echo '-- Installing'$$i ; \
	  $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR)/$$i; \
	done
