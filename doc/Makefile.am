DOC_MODULE = fontconfig
DOC2HTML = docbook2html
DOC2TXT = docbook2txt
DOC2MAN = docbook2man

TXT = fontconfig-user.txt fontconfig-devel.txt
HTML_FILES = fontconfig-user.html
HTML_DIRS = fontconfig-devel
SGML = fontconfig-user.sgml fontconfig-devel.sgml

DOC_FUNCS_FNCS=\
	fcatomic.fncs \
	fcblanks.fncs \
	fccharset.fncs \
	fcconfig.fncs \
	fcconstant.fncs \
	fcfile.fncs \
	fcfontset.fncs \
	fcfreetype.fncs \
	fcinit.fncs \
	fcmatrix.fncs \
	fcobjectset.fncs \
	fcobjecttype.fncs \
	fcpattern.fncs \
	fcstring.fncs \
	fcstrset.fncs \
	fcvalue.fncs
	
DOC_FUNCS_SGML=\
	fcatomic.sgml \
	fcblanks.sgml \
	fccharset.sgml \
	fcconfig.sgml \
	fcconstant.sgml \
	fcfile.sgml \
	fcfontset.sgml \
	fcfreetype.sgml \
	fcinit.sgml \
	fcmatrix.sgml \
	fcobjectset.sgml \
	fcobjecttype.sgml \
	fcpattern.sgml \
	fcstring.sgml \
	fcstrset.sgml \
	fcvalue.sgml

man5_MANS=fonts-conf.5

man3_MANS= \
FcAtomicCreate.3 FcAtomicDeleteNew.3 FcAtomicDestroy.3 FcAtomicLock.3 \
FcAtomicNewFile.3 FcAtomicOrigFile.3 FcAtomicReplaceOrig.3 FcAtomicUnlock.3 \
FcBlanksAdd.3 FcBlanksCreate.3 FcBlanksDestroy.3 FcBlanksIsMember.3 \
FcCharSetAddChar.3 FcCharSetCopy.3 FcCharSetCount.3 FcCharSetCreate.3 \
FcCharSetDestroy.3 FcCharSetEqual.3 FcCharSetFirstPage.3 FcCharSetHasChar.3 \
FcCharSetIntersect.3 FcCharSetIntersectCount.3 FcCharSetIsSubset.3 \
FcCharSetNextPage.3 FcCharSetSubtract.3 FcCharSetSubtractCount.3 \
FcCharSetUnion.3 FcConfigAppFontAddDir.3 FcConfigAppFontAddFile.3 \
FcConfigAppFontClear.3 FcConfigBuildFonts.3 FcConfigCreate.3 \
FcConfigDestroy.3 FcConfigFilename.3 FcConfigGetBlanks.3 FcConfigGetCache.3 \
FcConfigGetConfigDirs.3 FcConfigGetConfigFiles.3 FcConfigGetCurrent.3 \
FcConfigGetFontDirs.3 FcConfigGetFonts.3 FcConfigGetRescanInverval.3 \
FcConfigParseAndLoad.3 FcConfigSetCurrent.3 FcConfigSetRescanInverval.3 \
FcConfigSubstitute.3 FcConfigSubstituteWithPat.3 FcConfigUptoDate.3 \
FcDefaultSubstitute.3 FcDirCacheValid.3 FcDirSave.3 FcDirScan.3 FcFileScan.3 \
FcFontList.3 FcFontMatch.3 FcFontRenderPrepare.3 FcFontSetAdd.3 \
FcFontSetCreate.3 FcFontSetDestroy.3 FcFontSort.3 FcFreeTypeCharIndex.3 \
FcFreeTypeCharSet.3 FcFreeTypeQuery.3 FcGetVersion.3 FcInit.3 \
FcInitBringUptoDate.3 FcInitLoadConfig.3 FcInitLoadConfigAndFonts.3 \
FcInitReinitialize.3 FcMatrixCopy.3 FcMatrixEqual.3 FcMatrixInit.3 \
FcMatrixMultiply.3 FcMatrixRotate.3 FcMatrixScale.3 FcMatrixShear.3 \
FcNameConstant.3 FcNameGetConstant.3 FcNameGetObjectType.3 FcNameParse.3 \
FcNameRegisterConstants.3 FcNameRegisterObjectTypes.3 FcNameUnparse.3 \
FcNameUnregisterConstants.3 FcNameUnregisterObjectTypes.3 FcObjectSetAdd.3 \
FcObjectSetBuild.3 FcObjectSetCreate.3 FcObjectSetDestroy.3 \
FcPatternAdd-Type.3 FcPatternAdd.3 FcPatternAddWeak.3 FcPatternBuild.3 \
FcPatternCreate.3 FcPatternDel.3 FcPatternDestroy.3 FcPatternEqual.3 \
FcPatternEqualSubset.3 FcPatternGet-Type.3 FcPatternGet.3 FcPatternHash.3 \
FcPatternPrint.3 FcStrBasename.3 FcStrCmpIgnoreCase.3 FcStrCopy.3 \
FcStrCopyFilename.3 FcStrDirname.3 FcStrListCreate.3 FcStrListDone.3 \
FcStrListNext.3 FcStrSetAdd.3 FcStrSetAddFilename.3 FcStrSetCreate.3 \
FcStrSetDel.3 FcStrSetDestroy.3 FcStrSetMember.3 FcUcs4ToUtf8.3 FcUtf16Len.3 \
FcUtf16ToUcs4.3 FcUtf8Len.3 FcUtf8ToUcs4.3 FcValueDestroy.3 FcValueSave.3

noinst_PROGRAMS=edit-sgml
edit_sgml_SOURCES=edit-sgml.c

DOCDIR=@DOCDIR@

SUFFIXES=.fncs .sgml .txt .html

.fncs.sgml:
	$(RM) $@
	./edit-sgml func.sgml < $*.fncs > $*.sgml

.sgml.txt:
	$(RM) $@
	$(DOC2TXT) $*.sgml

EXTRA_DIST = $(TXT) $(HTML_FILES) $(HTML_DIR)/* $(SGML) $(man3_MANS)
	
if ENABLE_DOCS
MAN3_FILES=$(man3_MANS)
MAN5_FILES=$(man5_MANS)
DOC_FILES=$(TXT) $(HTML_FILES)
DOC_DIRS=$(HTML_DIRS)
DOCS=$(MAN3_FILES) $(MAN5_FILES) $(DOC_FILES) $(DOC_DIRS)
all-local: all-local-docs
clean-local: clean-local-docs
install-data-local: install-local-docs
else
all-local:
clean-local:
install-data-local:
endif

$(MAN3_FILES): func.refs

func.refs: fontconfig-devel.sgml $(DOC_FUNCS_SGML)
	$(RM) func.refs
	$(DOC2MAN) fontconfig-devel.sgml
	mv manpage.refs func.refs
	$(RM) manpage.links

$(DOC_FUNCS_SGML): edit-sgml func.sgml

$(MAN5_FILES): fontconfig-user.sgml
	$(RM) file.refs
	$(DOC2MAN) -u fontconfig-user.sgml
	$(RM) manpage.refs manpage.links

all-local-docs: $(DOCS)

clean-local-docs:
	$(RM) $(MAN3_FILES) $(MAN5_FILES) $(DOC_FILES) $(DOC_FUNCS_SGML) func.refs
	$(RM) -r $(DOC_DIRS)

fontconfig-devel: fontconfig-devel.sgml $(DOCS_FUNCS_SGML)
	$(RM) -r fontconfig-devel
	$(DOC2HTML) -o fontconfig-devel fontconfig-devel.sgml

fontconfig-user.html: fontconfig-user.sgml
	$(DOC2HTML) -u fontconfig-user.sgml

install-local-docs:
	$(mkinstalldirs) $(DESTDIR)$(DOCDIR)
	for i in $(DOC_FILES); do \
	  echo '-- Installing '$$i ; \
	  $(INSTALL_DATA) $$i $(DESTDIR)$(DOCDIR)/$$i; \
	done
	for i in $(DOC_DIRS); do \
	  $(mkinstalldirs) $(DESTDIR)$(DOCDIR)/$$i ; \
	  for f in $$i/*; do \
	    echo '-- Installing '$$f ; \
	    $(INSTALL_DATA) $$f $(DESTDIR)$(DOCDIR)/$$f; \
	  done \
	done
