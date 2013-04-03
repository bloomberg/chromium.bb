<?xml version="1.0" ?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml" indent="yes" encoding="UTF-8"/>
<xsl:output method="xml" encoding="UTF-8" indent="yes" />

<xsl:template match="/">
  <!-- insert docbook's DOCTYPE blurb -->
    <xsl:text disable-output-escaping = "yes"><![CDATA[
<!DOCTYPE appendix PUBLIC "-//OASIS//DTD DocBook XML V4.5//EN" "http://www.oasis-open.org/docbook/xml/4.5/docbookx.dtd" [
  <!ENTITY % BOOK_ENTITIES SYSTEM "Wayland.ent">
%BOOK_ENTITIES;
]>
]]></xsl:text>

  <appendix id="appe-Wayland-Protocol">
    <title>Wayland Protocol Specification</title>
    <xsl:apply-templates select="protocol/copyright" />

    <xsl:apply-templates select="protocol/interface" />
  </appendix>
</xsl:template>

<!-- Break text blocks separated by two new lines into paragraphs -->
<xsl:template name="break">
     <xsl:param name="text" />
     <xsl:param name="linebreak" select="'&#10;&#10;'" />
     <xsl:choose>
       <xsl:when test="contains($text,$linebreak)">
         <para>
           <xsl:value-of select="substring-before($text,$linebreak)" />
         </para>
         <xsl:call-template name="break">
           <xsl:with-param name="text" select="substring-after($text,$linebreak)" />
         </xsl:call-template>
       </xsl:when>
       <xsl:otherwise>
         <para><xsl:value-of select="$text" /></para>
       </xsl:otherwise>
     </xsl:choose>
</xsl:template>

<!-- Copyright blurb -->
<xsl:template match="copyright">
  <para>
    <literallayout>
      <xsl:value-of select="." disable-output-escaping="yes"/>
    </literallayout>
  </para>
</xsl:template>

<!-- Interface descriptions -->
<xsl:template match="interface" >
  <section>
    <xsl:attribute name="id">protocol-spec-<xsl:value-of select="name()"/>-<xsl:value-of select="@name" />
    </xsl:attribute>
    <title>
      <xsl:value-of select="@name" />
      <!-- only show summary if it exists -->
      <xsl:if test="description/@summary">
	- <xsl:value-of select="description/@summary" />
      </xsl:if>
    </title>
    <xsl:call-template name="break">
      <xsl:with-param name="text" select="description" />
    </xsl:call-template>
    <xsl:if test="request">
      <section>
        <title>Requests provided by <xsl:value-of select="@name" /></title>
        <xsl:apply-templates select="request" />
      </section>
    </xsl:if>
    <xsl:if test="event">
      <section>
        <title>Events provided by <xsl:value-of select="@name" /></title>
        <xsl:apply-templates select="event" />
      </section>
    </xsl:if>
    <xsl:if test="enum">
      <section>
        <title>Enums provided by <xsl:value-of select="@name" /></title>
      <xsl:apply-templates select="enum" />
      </section>
    </xsl:if>
  </section>
</xsl:template>

<!-- table contents for request/event arguments or enum values -->
<xsl:template match="arg|entry">
  <varlistentry>
    <term><xsl:value-of select="@name"/></term>
    <listitem>
        <xsl:if test="name() = 'arg'" >
          <para>Type: <xsl:value-of select="@type"/></para>
        </xsl:if>
        <xsl:if test="name() = 'entry'" >
          <para>Value: <xsl:value-of select="@value"/></para>
        </xsl:if>
        <xsl:if test="@summary" >
          <para>
            <xsl:value-of select="@summary"/>
          </para>
        </xsl:if>
    </listitem>
  </varlistentry>
</xsl:template>

<!-- Request/event list -->
<xsl:template match="request|event|enum">
  <section>
    <xsl:attribute name="id">protocol-spec-interface-<xsl:value-of select="../@name"/>-<xsl:value-of select="name()"/>-<xsl:value-of select="@name"/></xsl:attribute>
    <title>
      <xsl:value-of select="../@name"/>::<xsl:value-of select="@name" />
      <xsl:if test="description/@summary">
        - <xsl:value-of select="description/@summary" />
      </xsl:if>
    </title>
    <xsl:call-template name="break">
      <xsl:with-param name="text" select="description" />
    </xsl:call-template>
    <xsl:if test="arg">
      <variablelist>
        <title><xsl:value-of select="../@name"/>::<xsl:value-of select="@name" /> arguments</title>
        <xsl:apply-templates select="arg"/>
      </variablelist>
    </xsl:if>
    <xsl:if test="entry">
      <variablelist>
        <title><xsl:value-of select="../@name"/>::<xsl:value-of select="@name" /> values</title>
          <xsl:apply-templates select="entry"/>
      </variablelist>
    </xsl:if>
  </section>
</xsl:template>
</xsl:stylesheet>

<!-- vim: set expandtab shiftwidth=2: -->
